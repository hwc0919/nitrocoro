/**
 * @file CoroScheduler.h
 * @brief Native coroutine scheduler - replaces EventLoop for coroutine-first design
 */
#pragma once

#include <atomic>
#include <cassert>
#include <chrono>
#include <coroutine>
#include <memory>
#include <queue>
#include <unordered_map>
#include <unordered_set>

#include "CoroTraits.h"
#include "MpscQueue.h"

namespace my_coro
{
class Scheduler;
class IoChannel;

enum class IoOp
{
    Read,
    Write
};

using TimePoint = std::chrono::steady_clock::time_point;

struct [[nodiscard]] TimerAwaiter
{
    Scheduler * sched_;
    TimePoint when_;
    std::coroutine_handle<> handle_;

    bool await_ready() const noexcept { return false; }
    void await_suspend(std::coroutine_handle<> h) noexcept;
    void await_resume() noexcept {}
};

struct AsyncTask
{
    struct promise_type;
    using handle_type = std::coroutine_handle<promise_type>;

    AsyncTask() = default;

    AsyncTask(handle_type h) : coro_(h)
    {
    }

    AsyncTask(const AsyncTask &) = delete;

    AsyncTask(AsyncTask && other) noexcept
    {
        coro_ = other.coro_;
        other.coro_ = nullptr;
    }

    AsyncTask & operator=(const AsyncTask &) = delete;

    AsyncTask & operator=(AsyncTask && other) noexcept
    {
        if (std::addressof(other) == this)
            return *this;

        coro_ = other.coro_;
        other.coro_ = nullptr;
        return *this;
    }

    struct promise_type
    {
        AsyncTask get_return_object() noexcept { return { handle_type::from_promise(*this) }; }
        std::suspend_always initial_suspend() const noexcept { return {}; }
        void unhandled_exception() { std::terminate(); }
        void return_void() noexcept {}
        std::suspend_never final_suspend() const noexcept { return {}; }
    };

    handle_type coro_;
};

class Scheduler
{
public:
    Scheduler();
    ~Scheduler();

    Scheduler(const Scheduler &) = delete;
    Scheduler & operator=(const Scheduler &) = delete;

    static Scheduler * current() noexcept;

    void run();
    void stop();

    void registerIoChannel(IoChannel *);
    void unregisterIoChannel(IoChannel *);
    void updateChannel(IoChannel *);

    TimerAwaiter sleep_for(double seconds);
    TimerAwaiter sleep_until(TimePoint when);

    void schedule(std::coroutine_handle<> coro);

    template <typename Coro>
    void spawn(Coro && coro)
    {
        auto taskFunc = [](std::decay_t<Coro> coro) -> AsyncTask {
            static_assert(is_awaitable_v<std::decay_t<decltype(coro())>>);
            co_await coro();
            co_return;
        };
        auto task = taskFunc(std::forward<Coro>(coro));
        if (task.coro_)
            schedule(task.coro_);
    }

    void register_timer(TimePoint when, std::coroutine_handle<> coro);

private:
    static thread_local Scheduler * current_;

    int epoll_fd_{ -1 };
    int wakeup_fd_{ -1 }; // eventfd 用于唤醒 epoll
    std::atomic<bool> running_{ false };

    MpscQueue<std::coroutine_handle<>> ready_queue_;

    struct Timer
    {
        TimePoint when;
        std::coroutine_handle<> coro;

        bool operator>(const Timer & other) const
        {
            return when > other.when;
        }
    };
    std::priority_queue<Timer, std::vector<Timer>, std::greater<Timer>> timers_;
    MpscQueue<Timer> pending_timers_;

    void resume_ready_coros();
    void process_io_events(int timeout_ms);
    void process_timers();
    int64_t get_next_timeout();
    void wakeup();

    std::unordered_map<int, IoChannel *> ioChannels_;

    std::unique_ptr<IoChannel> wakeupChannel_;
};

} // namespace my_coro
