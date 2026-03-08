/**
 * @file PooledConnection.cc
 * @brief PooledConnection implementation
 */
#include "PooledConnection.h"
#include "PgConnectionImpl.h"
#include "PoolState.h"

namespace nitrocoro::pg
{

PooledConnection::PooledConnection(std::unique_ptr<PgConnectionImpl> impl, std::weak_ptr<PoolState> state)
    : impl_(std::move(impl)), state_(std::move(state))
{
    impl_->setBrokenHandler([state = state_, detached = detached_] {
        if (!detached->test_and_set())
            PoolState::detachConnection(state);
    });
}

PooledConnection::~PooledConnection()
{
    if (impl_ && !detached_->test_and_set())
        PoolState::returnConnection(state_, std::move(impl_));
}

Scheduler * PooledConnection::scheduler() const
{
    return impl_->scheduler();
}

bool PooledConnection::isAlive() const
{
    return impl_->isAlive();
}

Task<PgResult> PooledConnection::query(std::string_view sql, std::vector<PgValue> params, CancelToken cancelToken)
{
    return impl_->query(sql, std::move(params), cancelToken);
}

Task<> PooledConnection::execute(std::string_view sql, std::vector<PgValue> params, CancelToken cancelToken)
{
    return impl_->execute(sql, std::move(params), cancelToken);
}

} // namespace nitrocoro::pg
