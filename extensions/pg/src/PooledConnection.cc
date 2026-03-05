/**
 * @file PooledConnection.cc
 * @brief PooledConnection implementation
 */
#include "nitrocoro/pg/PooledConnection.h"
#include "PoolState.h"
#include "nitrocoro/pg/PgConnection.h"

namespace nitrocoro::pg
{

PooledConnection::PooledConnection(std::unique_ptr<PgConnection> conn, std::weak_ptr<PoolState> state)
    : conn_(std::move(conn)), state_(std::move(state))
{
}

PooledConnection::~PooledConnection()
{
    if (conn_)
        PoolState::returnConnection(state_, std::move(conn_));
}

Task<PgResult> PooledConnection::query(std::string_view sql, std::vector<PgValue> params)
{
    return conn_->query(sql, std::move(params));
}

Task<> PooledConnection::execute(std::string_view sql, std::vector<PgValue> params)
{
    return conn_->execute(sql, std::move(params));
}

} // namespace nitrocoro::pg
