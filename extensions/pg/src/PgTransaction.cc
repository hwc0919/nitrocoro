/**
 * @file PgTransaction.cc
 * @brief PgTransaction implementation
 */
#include "nitrocoro/pg/PgTransaction.h"
#include "PoolState.h"
#include "nitrocoro/pg/PgConnection.h"
#include "nitrocoro/pg/PgResult.h"
#include "nitrocoro/pg/PooledConnection.h"
#include <nitrocoro/utils/Debug.h>

namespace nitrocoro::pg
{

Task<std::unique_ptr<PgTransaction>> PgTransaction::begin(std::unique_ptr<PooledConnection> pooled)
{
    co_await pooled->conn_->execute("BEGIN");
    auto state = std::move(pooled->state_);
    auto conn = std::move(pooled->conn_);
    co_return std::unique_ptr<PgTransaction>(new PgTransaction(std::move(conn), std::move(state)));
}

Task<std::unique_ptr<PgTransaction>> PgTransaction::begin(std::unique_ptr<PgConnection> conn)
{
    co_await conn->execute("BEGIN");
    co_return std::unique_ptr<PgTransaction>(new PgTransaction(std::move(conn), {}));
}

PgTransaction::PgTransaction(std::unique_ptr<PgConnection> conn, std::weak_ptr<PoolState> poolState)
    : conn_(std::move(conn)), poolState_(std::move(poolState))
{
}

PgTransaction::~PgTransaction()
{
    if (conn_)
    {
        auto conn = std::move(conn_);
        auto poolState = std::move(poolState_);
        conn->scheduler()->spawn([conn = std::move(conn),
                                  poolState = std::move(poolState),
                                  needRollback = !done_]() mutable -> Task<> {
            if (needRollback)
            {
                try
                {
                    co_await conn->execute("ROLLBACK");
                    NITRO_TRACE("PgTransaction: auto rollback successful\n");
                }
                catch (const std::exception & e)
                {
                    NITRO_ERROR("PgTransaction: auto rollback failed: %s\n", e.what());
                }
            }
            PoolState::returnConnection(poolState, std::move(conn));
        });
    }
}

Task<PgResult> PgTransaction::query(std::string_view sql, std::vector<PgValue> params)
{
    if (done_)
        throw std::logic_error("Transaction already finished");
    co_return co_await conn_->query(sql, std::move(params));
}

Task<> PgTransaction::execute(std::string_view sql, std::vector<PgValue> params)
{
    if (done_)
        throw std::logic_error("Transaction already finished");
    co_await conn_->execute(sql, std::move(params));
}

Task<> PgTransaction::commit()
{
    if (done_)
        throw std::logic_error("Transaction already finished");
    co_await conn_->execute("COMMIT");
    done_ = true;
}

Task<> PgTransaction::rollback()
{
    if (done_)
        throw std::logic_error("Transaction already finished");
    co_await conn_->execute("ROLLBACK");
    done_ = true;
}

std::unique_ptr<PgConnection> PgTransaction::release()
{
    if (!done_)
        throw std::logic_error("Cannot release connection before commit/rollback");
    PoolState::detachConnection(poolState_);
    poolState_ = {};
    return std::move(conn_);
}

} // namespace nitrocoro::pg
