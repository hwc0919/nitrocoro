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

class PgTransaction;

class PooledConnection
{
public:
    PooledConnection(std::shared_ptr<PgConnection> conn, std::function<void(std::shared_ptr<PgConnection>)> returnFn);
    ~PooledConnection() noexcept;

    PooledConnection(const PooledConnection &) = delete;
    PooledConnection & operator=(const PooledConnection &) = delete;
    PooledConnection(PooledConnection &&) = default;
    PooledConnection & operator=(PooledConnection &&) = default;

    PgConnection * operator->() const { return conn_.get(); }
    PgConnection & operator*() const { return *conn_; }
    explicit operator bool() const noexcept { return conn_ != nullptr; }

private:
    std::shared_ptr<PgConnection> conn_;
    std::function<void(std::shared_ptr<PgConnection>)> returnFn_;
};

class PgPool
{
public:
    using Factory = std::function<Task<std::shared_ptr<PgConnection>>()>;

    PgPool(size_t maxSize, Factory factory, Scheduler * scheduler = Scheduler::current())
        : factory_(std::move(factory)), scheduler_(scheduler), maxSize_(maxSize)
    {
    }
    PgPool(const PgPool &) = delete;
    PgPool & operator=(const PgPool &) = delete;

    [[nodiscard]] Task<PooledConnection> acquire();
    size_t idleCount() const { return idle_.size(); }

    [[nodiscard]] Task<PgTransaction> newTransaction();

private:
    void returnConnection(std::shared_ptr<PgConnection> conn) noexcept;

    Factory factory_;
    Scheduler * scheduler_;
    size_t maxSize_;
    size_t totalCount_{ 0 };
    Mutex mutex_;
    std::queue<std::shared_ptr<PgConnection>> idle_;
    std::queue<Promise<std::shared_ptr<PgConnection>>> waiters_;
};

} // namespace nitrocoro::pg
