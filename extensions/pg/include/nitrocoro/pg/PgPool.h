/**
 * @file PgPool.h
 * @brief Coroutine-aware PostgreSQL connection pool
 */
#pragma once

#include "nitrocoro/pg/PooledConnection.h" // for user convenience
#include <nitrocoro/core/Scheduler.h>
#include <nitrocoro/core/Task.h>

#include <functional>
#include <memory>

namespace nitrocoro::pg
{

struct PoolState;
class PgConnection;
class PgTransaction;
class PooledConnection;

class PgPool
{
public:
    using Factory = std::function<Task<std::unique_ptr<PgConnection>>()>;

    PgPool(size_t maxSize, Factory factory, Scheduler * scheduler = Scheduler::current());
    ~PgPool();
    PgPool(const PgPool &) = delete;
    PgPool & operator=(const PgPool &) = delete;

    [[nodiscard]] Task<std::unique_ptr<PooledConnection>> acquire();
    [[nodiscard]] Task<std::unique_ptr<PgTransaction>> newTransaction();
    size_t idleCount() const;

private:
    std::shared_ptr<PoolState> state_;
    Factory factory_;
};

} // namespace nitrocoro::pg
