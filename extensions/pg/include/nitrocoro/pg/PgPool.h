/**
 * @file PgPool.h
 * @brief Coroutine-aware PostgreSQL connection pool
 */
#pragma once

#include <nitrocoro/core/Future.h>
#include <nitrocoro/core/Mutex.h>
#include <nitrocoro/core/Scheduler.h>
#include <nitrocoro/core/Task.h>
#include <nitrocoro/pg/PgConnection.h>

#include <functional>
#include <memory>
#include <queue>

namespace nitrocoro::pg
{

using nitrocoro::Mutex;
using nitrocoro::Promise;
using nitrocoro::Scheduler;
using nitrocoro::Task;

class PooledConnection
{
public:
    PooledConnection(std::shared_ptr<PgConnection> conn, std::function<void(std::shared_ptr<PgConnection>)> returnFn)
        : conn_(std::move(conn)), returnFn_(std::move(returnFn))
    {
    }

    PooledConnection(const PooledConnection &) = delete;
    PooledConnection & operator=(const PooledConnection &) = delete;
    PooledConnection(PooledConnection &&) = default;
    PooledConnection & operator=(PooledConnection &&) = default;

    ~PooledConnection()
    {
        if (conn_)
            returnFn_(std::move(conn_));
    }

    PgConnection * operator->() const { return conn_.get(); }
    PgConnection & operator*() const { return *conn_; }

private:
    std::shared_ptr<PgConnection> conn_;
    std::function<void(std::shared_ptr<PgConnection>)> returnFn_;
};

class PgPool
{
public:
    using Factory = std::function<Task<std::shared_ptr<PgConnection>>()>;

    PgPool(size_t size, Factory factory, Scheduler * scheduler = Scheduler::current())
        : factory_(std::move(factory)), scheduler_(scheduler), size_(size)
    {
    }

    PgPool(const PgPool &) = delete;
    PgPool & operator=(const PgPool &) = delete;

    Task<> init()
    {
        for (size_t i = 0; i < size_; ++i)
            idle_.push(co_await factory_());
    }

    [[nodiscard]] Task<PooledConnection> acquire()
    {
        [[maybe_unused]] auto lock = co_await mutex_.scoped_lock();

        while (idle_.empty())
        {
            Promise<> promise(scheduler_);
            auto future = promise.get_future();
            waiters_.push(std::move(promise));
            lock.unlock();
            co_await future.get();
            lock = co_await mutex_.scoped_lock();
        }

        auto conn = std::move(idle_.front());
        idle_.pop();

        co_return PooledConnection(std::move(conn), [this](std::shared_ptr<PgConnection> c) {
            returnConnection(std::move(c));
        });
    }

    size_t idleCount() const { return idle_.size(); }

private:
    void returnConnection(std::shared_ptr<PgConnection> conn)
    {
        scheduler_->dispatch([this, conn = std::move(conn)]() mutable {
            idle_.push(std::move(conn));
            if (!waiters_.empty())
            {
                waiters_.front().set_value();
                waiters_.pop();
            }
        });
    }

    Factory factory_;
    Scheduler * scheduler_;
    size_t size_;
    Mutex mutex_;
    std::queue<std::shared_ptr<PgConnection>> idle_;
    std::queue<Promise<>> waiters_;
};

} // namespace nitrocoro::pg
