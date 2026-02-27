/**
 * @file pg_test.cc
 * @brief Integration tests for PgConnection and PgPool
 *
 * Requires a running PostgreSQL instance. Set the connection string via
 * environment variable PG_CONN_STR, e.g.:
 *   PG_CONN_STR="host=localhost dbname=test user=postgres password=secret" ./pg_test
 */
#include <nitrocoro/pg/PgConnection.h>
#include <nitrocoro/pg/PgPool.h>
#include <nitrocoro/pg/PgTransaction.h>
#include <nitrocoro/testing/Test.h>

#include <cstdlib>

using namespace nitrocoro;
using namespace nitrocoro::pg;

static std::string connStr()
{
    const char * env = std::getenv("PG_CONN_STR");
    return env ? env : "host=localhost dbname=test user=postgres";
}

Task<std::shared_ptr<PgConnection>> makeConn()
{
    co_return co_await PgConnection::connect(connStr());
}

// ── Tests ─────────────────────────────────────────────────────────────────────

NITRO_TEST(connect)
{
    auto conn = co_await PgConnection::connect(connStr());
    NITRO_REQUIRE(conn != nullptr);
    NITRO_CHECK(conn->isAlive());
}

NITRO_TEST(simple_query)
{
    auto conn = co_await PgConnection::connect(connStr());
    auto result = co_await conn->query("SELECT 1 AS val, 'hello' AS str");
    NITRO_CHECK_EQ(result->rowCount(), 1);
    NITRO_CHECK_EQ(result->colCount(), 2);
    NITRO_CHECK(result->colName(0) == "val");
    NITRO_CHECK(result->colName(1) == "str");
    NITRO_CHECK(std::get<int64_t>(result->get(0, 0)) == 1);
    NITRO_CHECK(std::get<std::string>(result->get(0, 1)) == "hello");
}

NITRO_TEST(params)
{
    auto conn = co_await PgConnection::connect(connStr());
    std::vector<PgValue> params{ int64_t(3), int64_t(4) };
    auto result = co_await conn->query("SELECT $1::int8 + $2::int8 AS sum", std::move(params));
    NITRO_CHECK(std::get<int64_t>(result->get(0, 0)) == 7);
}

NITRO_TEST(null_value)
{
    auto conn = co_await PgConnection::connect(connStr());
    auto result = co_await conn->query("SELECT NULL::text AS n");
    NITRO_CHECK(std::holds_alternative<std::monostate>(result->get(0, 0)));
}

NITRO_TEST(bool_value)
{
    auto conn = co_await PgConnection::connect(connStr());
    auto result = co_await conn->query("SELECT true AS t, false AS f");
    NITRO_CHECK(std::get<bool>(result->get(0, 0)) == true);
    NITRO_CHECK(std::get<bool>(result->get(0, 1)) == false);
}

NITRO_TEST(transaction)
{
    auto conn = co_await PgConnection::connect(connStr());
    co_await conn->execute("CREATE TEMP TABLE tx_test (v INT)");

    co_await conn->begin();
    co_await conn->execute("INSERT INTO tx_test VALUES (42)");
    co_await conn->rollback();
    auto result = co_await conn->query("SELECT COUNT(*) FROM tx_test");
    NITRO_CHECK(std::get<int64_t>(result->get(0, 0)) == 0);

    co_await conn->begin();
    co_await conn->execute("INSERT INTO tx_test VALUES (99)");
    co_await conn->commit();
    result = co_await conn->query("SELECT v FROM tx_test");
    NITRO_CHECK(std::get<int64_t>(result->get(0, 0)) == 99);
}

NITRO_TEST(pool)
{
    PgPool pool(2, makeConn);
    {
        auto c1 = co_await pool.acquire();
        auto result = co_await c1->query("SELECT 'pool' AS src");
        NITRO_CHECK(std::get<std::string>(result->get(0, 0)) == "pool");
    }
    co_await Scheduler::current()->sleep_for(0.5);
    NITRO_CHECK_EQ(pool.idleCount(), 1);
}

NITRO_TEST(pool_waiter)
{
    PgPool pool(1, makeConn);
    auto c1 = co_await pool.acquire();
    NITRO_CHECK_EQ(pool.idleCount(), 0);

    bool waiterRan = false;
    Scheduler::current()->spawn([TEST_CTX, &pool, &waiterRan]() mutable -> Task<> {
        auto c2 = co_await pool.acquire();
        waiterRan = true;
        auto result = co_await c2->query("SELECT 1");
        NITRO_CHECK_EQ(result->rowCount(), 1);
    });

    {
        auto drop = std::move(c1);
    }
    co_await Scheduler::current()->sleep_for(0.1);
    NITRO_CHECK(waiterRan);
}

NITRO_TEST(transaction_raii_rollback)
{
    PgPool pool(1, makeConn);
    co_await (co_await pool.acquire())->execute("CREATE TEMP TABLE tx_raii_test (v INT)");
    {
        auto tx = co_await pool.newTransaction();
        co_await tx.execute("INSERT INTO tx_raii_test VALUES (1)");
    }
    co_await Scheduler::current()->sleep_for(std::chrono::seconds(1));
    auto conn = co_await pool.acquire();
    auto result = co_await conn->query("SELECT COUNT(*) FROM tx_raii_test");
    NITRO_CHECK(std::get<int64_t>(result->get(0, 0)) == 0);
}

NITRO_TEST(transaction_commit)
{
    PgPool pool(1, makeConn);
    co_await (co_await pool.acquire())->execute("CREATE TEMP TABLE tx_commit_test (v INT)");
    {
        auto tx = co_await pool.newTransaction();
        co_await tx.execute("INSERT INTO tx_commit_test VALUES (42)");
        co_await tx.commit();
    }
    auto conn = co_await pool.acquire();
    auto result = co_await conn->query("SELECT v FROM tx_commit_test");
    NITRO_CHECK(std::get<int64_t>(result->get(0, 0)) == 42);
}

NITRO_TEST(transaction_pool_return)
{
    PgPool pool(1, makeConn);
    {
        auto tx = co_await pool.newTransaction();
        NITRO_CHECK_EQ(pool.idleCount(), 0);
        co_await tx.rollback();
    }
    co_await Scheduler::current()->sleep_for(0.5);
    NITRO_CHECK_EQ(pool.idleCount(), 1);
}

int main()
{
    return nitrocoro::test::run_all();
}
