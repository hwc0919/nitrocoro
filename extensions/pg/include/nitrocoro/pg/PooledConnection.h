/**
 * @file PooledConnection.h
 * @brief RAII handle for a connection borrowed from PgPool
 */
#pragma once

#include "nitrocoro/pg/PgResult.h"
#include <nitrocoro/core/Task.h>

#include <memory>
#include <string_view>
#include <vector>

namespace nitrocoro::pg
{

struct PoolState;
class PgPool;
class PgConnection;
class PgTransaction;

class PooledConnection
{
public:
    ~PooledConnection();

    PooledConnection(const PooledConnection &) = delete;
    PooledConnection & operator=(const PooledConnection &) = delete;
    PooledConnection(PooledConnection &&) = delete;
    PooledConnection & operator=(PooledConnection &&) = delete;

    Task<PgResult> query(std::string_view sql, std::vector<PgValue> params = {});
    Task<> execute(std::string_view sql, std::vector<PgValue> params = {});

private:
    friend class PgPool;
    friend class PgTransaction;
    PooledConnection(std::unique_ptr<PgConnection> conn, std::weak_ptr<PoolState> state);

    std::unique_ptr<PgConnection> conn_;
    std::weak_ptr<PoolState> state_;
};

} // namespace nitrocoro::pg
