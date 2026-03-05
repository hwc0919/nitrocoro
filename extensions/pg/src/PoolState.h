/**
 * @file PoolState.h
 * @brief Internal pool state shared between PgPool, PooledConnection, and PgTransaction
 */
#pragma once

#include "nitrocoro/pg/PgConnection.h"
#include <nitrocoro/core/Future.h>
#include <nitrocoro/core/Mutex.h>
#include <nitrocoro/core/Scheduler.h>
#include <nitrocoro/core/Task.h>

#include <memory>
#include <queue>

namespace nitrocoro::pg
{

using nitrocoro::Mutex;
using nitrocoro::Promise;
using nitrocoro::Scheduler;
using nitrocoro::Task;

struct PoolState
{
    Scheduler * scheduler;
    size_t maxSize;
    size_t totalCount = 0;
    Mutex mutex;
    std::queue<std::unique_ptr<PgConnection>> idle;
    std::queue<Promise<std::unique_ptr<PgConnection>>> waiters;

    static void returnConnection(const std::weak_ptr<PoolState> & state,
                                 std::unique_ptr<PgConnection> conn) noexcept;
    static void detachConnection(const std::weak_ptr<PoolState> & state) noexcept;
};

} // namespace nitrocoro::pg
