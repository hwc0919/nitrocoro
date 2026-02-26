/**
 * @file pg_test.cc
 * @brief Integration tests for PgConnection and PgPool
 *
 * Requires a running PostgreSQL instance. Set the connection string via
 * environment variable PG_CONN_STR, e.g.:
 *   PG_CONN_STR="host=localhost dbname=test user=postgres password=secret" ./pg_test
 */
#include <nitrocoro/core/Scheduler.h>
#include <nitrocoro/pg/PgConnection.h>
#include <nitrocoro/pg/PgPool.h>
#include <nitrocoro/pg/PgTransaction.h>
#include <nitrocoro/utils/Debug.h>

#include <cstdlib>
#include <iostream>

using namespace nitrocoro;
using namespace nitrocoro::pg;

static int passed = 0;
static int failed = 0;

#define ASSERT(cond, msg)                                        \
    do                                                           \
    {                                                            \
        if (cond)                                                \
        {                                                        \
            NITRO_INFO("[PASS] %s\n", msg);                      \
            ++passed;                                            \
        }                                                        \
        else                                                     \
        {                                                        \
            NITRO_ERROR("[FAIL] %s (line %d)\n", msg, __LINE__); \
            ++failed;                                            \
        }                                                        \
    } while (0)

static std::string connStr()
{
    const char * env = std::getenv("PG_CONN_STR");
    return env ? env : "host=localhost dbname=test user=postgres";
}

Task<std::shared_ptr<PgConnection>> makeConn()
{
    co_return co_await PgConnection::connect(connStr());
}

// ── test cases ────────────────────────────────────────────────────────────────

Task<> test_connect()
{
    NITRO_INFO("\n--- test_connect ---\n");
    auto conn = co_await PgConnection::connect(connStr());
    ASSERT(conn != nullptr, "connect returns non-null");
    ASSERT(conn->isAlive(), "connection is alive after connect");
}

Task<> test_simple_query()
{
    NITRO_INFO("\n--- test_simple_query ---\n");
    auto conn = co_await PgConnection::connect(connStr());
    auto result = co_await conn->query("SELECT 1 AS val, 'hello' AS str");
    ASSERT(result->rowCount() == 1, "rowCount == 1");
    ASSERT(result->colCount() == 2, "colCount == 2");
    ASSERT(result->colName(0) == "val", "col 0 name == val");
    ASSERT(result->colName(1) == "str", "col 1 name == str");
    ASSERT(std::get<int64_t>(result->get(0, 0)) == 1, "val == 1");
    ASSERT(std::get<std::string>(result->get(0, 1)) == "hello", "str == hello");
}

Task<> test_params()
{
    NITRO_INFO("\n--- test_params ---\n");
    auto conn = co_await PgConnection::connect(connStr());
    std::vector<PgValue> params{ int64_t(3), int64_t(4) };
    auto result = co_await conn->query("SELECT $1::int8 + $2::int8 AS sum", std::move(params));
    ASSERT(std::get<int64_t>(result->get(0, 0)) == 7, "3 + 4 == 7");
}

Task<> test_null()
{
    NITRO_INFO("\n--- test_null ---\n");
    auto conn = co_await PgConnection::connect(connStr());
    auto result = co_await conn->query("SELECT NULL::text AS n");
    ASSERT(std::holds_alternative<std::monostate>(result->get(0, 0)), "NULL maps to monostate");
}

Task<> test_bool()
{
    NITRO_INFO("\n--- test_bool ---\n");
    auto conn = co_await PgConnection::connect(connStr());
    auto result = co_await conn->query("SELECT true AS t, false AS f");
    ASSERT(std::get<bool>(result->get(0, 0)) == true, "true == true");
    ASSERT(std::get<bool>(result->get(0, 1)) == false, "false == false");
}

Task<> test_transaction()
{
    NITRO_INFO("\n--- test_transaction ---\n");
    auto conn = co_await PgConnection::connect(connStr());
    co_await conn->execute("CREATE TEMP TABLE tx_test (v INT)");

    co_await conn->begin();
    co_await conn->execute("INSERT INTO tx_test VALUES (42)");
    co_await conn->rollback();
    auto result = co_await conn->query("SELECT COUNT(*) FROM tx_test");
    ASSERT(std::get<int64_t>(result->get(0, 0)) == 0, "rollback: table is empty");

    co_await conn->begin();
    co_await conn->execute("INSERT INTO tx_test VALUES (99)");
    co_await conn->commit();
    result = co_await conn->query("SELECT v FROM tx_test");
    ASSERT(std::get<int64_t>(result->get(0, 0)) == 99, "commit: value persisted");
}

Task<> test_pool()
{
    NITRO_INFO("\n--- test_pool ---\n");
    PgPool pool(2, makeConn);

    {
        auto c1 = co_await pool.acquire();
        auto result = co_await c1->query("SELECT 'pool' AS src");
        ASSERT(std::get<std::string>(result->get(0, 0)) == "pool", "pool conn query works");
    }

    co_await Scheduler::current()->sleep_for(0.5);
    ASSERT(pool.idleCount() == 1, "1 idle after release");
}

Task<> test_pool_waiter()
{
    NITRO_INFO("\n--- test_pool_waiter ---\n");
    PgPool pool(1, makeConn);

    auto c1 = co_await pool.acquire();
    ASSERT(pool.idleCount() == 0, "0 idle, all borrowed");

    bool waiterRan = false;
    Promise<> done(Scheduler::current());
    auto doneFuture = done.get_future();

    Scheduler::current()->spawn([&pool, &waiterRan, done = std::move(done)]() mutable -> Task<> {
        auto c2 = co_await pool.acquire();
        waiterRan = true;
        auto result = co_await c2->query("SELECT 1");
        ASSERT(result->rowCount() == 1, "waiter: query succeeded after unblock");
        done.set_value();
    });

    {
        auto drop = std::move(c1);
    }
    co_await doneFuture.get();
    ASSERT(waiterRan, "waiter was unblocked after release");
}

Task<> test_transaction_raii_rollback()
{
    NITRO_INFO("\n--- test_transaction_raii_rollback ---\n");
    PgPool pool(1, makeConn);

    co_await (co_await pool.acquire())->execute("CREATE TEMP TABLE tx_raii_test (v INT)");

    {
        auto tx = co_await pool.newTransaction();
        NITRO_INFO("In transaction, inserting value...\n");
        co_await tx.execute("INSERT INTO tx_raii_test VALUES (1)");
        NITRO_INFO("Leaving scope without commit or rollback...\n");
        // tx goes out of scope without commit → auto rollback via spawn
    }

    // Give the spawned rollback a chance to run
    co_await Scheduler::current()->sleep_for(std::chrono::seconds(1));

    auto conn = co_await pool.acquire();
    auto result = co_await conn->query("SELECT COUNT(*) FROM tx_raii_test");
    ASSERT(std::get<int64_t>(result->get(0, 0)) == 0, "raii rollback: table is empty");
}

Task<> test_transaction_commit()
{
    NITRO_INFO("\n--- test_transaction_commit ---\n");
    PgPool pool(1, makeConn);

    co_await (co_await pool.acquire())->execute("CREATE TEMP TABLE tx_commit_test (v INT)");

    {
        auto tx = co_await pool.newTransaction();
        co_await tx.execute("INSERT INTO tx_commit_test VALUES (42)");
        co_await tx.commit();
    }

    auto conn = co_await pool.acquire();
    auto result = co_await conn->query("SELECT v FROM tx_commit_test");
    ASSERT(std::get<int64_t>(result->get(0, 0)) == 42, "commit: value persisted");
}

Task<> test_transaction_pool_return()
{
    NITRO_INFO("\n--- test_transaction_pool_return ---\n");
    PgPool pool(1, makeConn);

    {
        auto tx = co_await pool.newTransaction();
        ASSERT(pool.idleCount() == 0, "0 idle during transaction");
        co_await tx.rollback();
    }
    co_await Scheduler::current()->sleep_for(0.5);
    ASSERT(pool.idleCount() == 1, "1 idle after explicit rollback");
}


Task<> run_all()
{
    auto run = [](const char * name, Task<> task) -> Task<> {
        try
        {
            co_await task;
        }
        catch (const std::exception & e)
        {
            NITRO_ERROR("[FAIL] %s threw: %s\n", name, e.what());
            ++failed;
        }
    };

    co_await run("test_connect", test_connect());
    co_await run("test_simple_query", test_simple_query());
    co_await run("test_params", test_params());
    co_await run("test_null", test_null());
    co_await run("test_bool", test_bool());
    co_await run("test_transaction", test_transaction());
    co_await run("test_pool", test_pool());
    co_await run("test_pool_waiter", test_pool_waiter());
    co_await run("test_transaction_raii_rollback", test_transaction_raii_rollback());
    co_await run("test_transaction_commit", test_transaction_commit());
    co_await run("test_transaction_pool_return", test_transaction_pool_return());

    NITRO_INFO("\n=== Results: %d passed, %d failed ===\n", passed, failed);
    Scheduler::current()->stop();
}

int main()
{
    Scheduler scheduler;
    scheduler.spawn(run_all);
    scheduler.run();
    return failed > 0 ? 1 : 0;
}
