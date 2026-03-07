/**
 * @file PgConnection.cc
 * @brief PgConnection non-virtual overloads
 */
#include <nitrocoro/pg/PgConnection.h>

#include "PgConnectionImpl.h"

namespace nitrocoro::pg
{

Task<std::unique_ptr<PgConnection>> PgConnection::connect(const PgConnectConfig & config, Scheduler * scheduler)
{
    co_return co_await PgConnectionImpl::connect(config, scheduler);
}

Task<std::unique_ptr<PgConnection>> PgConnection::connect(std::string connStr, CancelToken cancelToken, Scheduler * scheduler)
{
    co_return co_await PgConnectionImpl::connect(std::move(connStr), cancelToken, scheduler);
}

Task<PgResult> PgConnection::query(std::string_view sql, std::vector<PgValue> params)
{
    co_return co_await query(sql, std::move(params), {});
}

Task<PgResult> PgConnection::query(std::string_view sql, CancelToken cancelToken)
{
    co_return co_await query(sql, {}, cancelToken);
}

Task<> PgConnection::execute(std::string_view sql, std::vector<PgValue> params)
{
    co_await execute(sql, std::move(params), {});
}

Task<> PgConnection::execute(std::string_view sql, CancelToken cancelToken)
{
    co_await execute(sql, {}, cancelToken);
}

} // namespace nitrocoro::pg
