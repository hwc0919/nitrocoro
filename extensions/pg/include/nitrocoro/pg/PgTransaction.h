/**
 * @file PgTransaction.h
 * @brief RAII PostgreSQL transaction with automatic rollback on destruction
 */
#pragma once
#include "nitrocoro/pg/PgConnection.h"
#include "nitrocoro/pg/PgResult.h"
#include <nitrocoro/core/Task.h>

#include <memory>
#include <string_view>
#include <vector>

namespace nitrocoro::pg
{

struct PoolState;
class PgConnection;
class PooledConnection;

class PgTransaction
{
public:
    static Task<std::unique_ptr<PgTransaction>> begin(std::unique_ptr<PooledConnection> conn);
    static Task<std::unique_ptr<PgTransaction>> begin(std::unique_ptr<PgConnection> conn);

    ~PgTransaction();

    PgTransaction(const PgTransaction &) = delete;
    PgTransaction & operator=(const PgTransaction &) = delete;
    PgTransaction(PgTransaction &&) = delete;
    PgTransaction & operator=(PgTransaction &&) = delete;

    Task<PgResult> query(std::string_view sql, std::vector<PgValue> params = {});
    Task<> execute(std::string_view sql, std::vector<PgValue> params = {});
    Task<> commit();
    Task<> rollback();

    std::unique_ptr<PgConnection> release();

private:
    PgTransaction(std::unique_ptr<PgConnection> conn, std::weak_ptr<PoolState> poolState);

    std::unique_ptr<PgConnection> conn_;
    std::weak_ptr<PoolState> poolState_;
    bool done_{ false };
};

} // namespace nitrocoro::pg
