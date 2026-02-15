/**
 * @file CoroScheduler.h
 * @brief Native coroutine scheduler - replaces EventLoop for coroutine-first design
 */
#pragma once

#include <atomic>
#include <chrono>
#include <coroutine>
#include <memory>
#include <queue>
#include <thread>
#include <unordered_map>
#include <unordered_set>

#include "CoroTraits.h"
#include "MpscQueue.h"

namespace my_coro
{
class Scheduler;
class IoChannel;

using TimePoint = std::chrono::steady_clock::time_point;

struct [[nodiscard]] TimerAwaiter
{
    TimerAwaiter(Scheduler * sched, TimePoint when) : sched_{ sched }, when_{ when } {};

    bool await_ready() const noexcept { return false; }
    void await_suspend(std::coroutine_handle<> h) noexcept;

    void await_resume() noexcept {}

private:
    Scheduler * sched_;
    TimePoint when_;
};

struct [[nodiscard]] SchedulerAwaiter
{
    Scheduler * scheduler_;

    bool await_ready() const noexcept;
    void await_suspend(std::coroutine_handle<> h) noexcept;
    void await_resume() noexcept {}
};

class Scheduler
{
public:
    using IoEventHandler = std::function<void(int, uint32_t)>;
    struct IoChannelContext
    {
        IoChannel * channel;
        IoEventHandler handler;
    };

    Scheduler();
    ~Scheduler();

    Scheduler(const Scheduler &) = delete;
    Scheduler & operator=(const Scheduler &) = delete;

    static Scheduler * current() noexcept;

    void run();
    void stop();

    bool isInOwnThread() const noexcept;

    void registerIoChannel(IoChannel *, IoEventHandler handler);
    void unregisterIoChannel(IoChannel *);
    void updateChannel(IoChannel *);

    TimerAwaiter sleep_for(double seconds);
    TimerAwaiter sleep_until(TimePoint when);
    SchedulerAwaiter run_here() noexcept;

    void schedule(std::coroutine_handle<> coro);
    void schedule_at(TimePoint when, std::coroutine_handle<> coro);

    template <typename Coro>
    void spawn(Coro && coro)
    {
        struct [[nodiscard]] FireAndForget
        {
            struct promise_type;
            using handle_type = std::coroutine_handle<promise_type>;

            handle_type handle_;

            FireAndForget(handle_type handle) : handle_{ handle } {}
            FireAndForget(const FireAndForget &) = delete;
            FireAndForget(FireAndForget &&) = delete;
            FireAndForget & operator=(const FireAndForget &) = delete;
            FireAndForget & operator=(FireAndForget &&) = delete;

            struct promise_type
            {
                FireAndForget get_return_object() noexcept { return handle_type::from_promise(*this); }
                std::suspend_always initial_suspend() noexcept { return {}; }
                void unhandled_exception() { std::terminate(); }
                void return_void() noexcept {}
                std::suspend_never final_suspend() const noexcept { return {}; }
            };
        };

        auto task = [](std::decay_t<Coro> coro) -> FireAndForget {
            static_assert(is_awaitable_v<std::decay_t<decltype(coro())>>);
            co_await coro();
            co_return;
        }(std::forward<Coro>(coro));
        schedule(task.handle_);
    }

private:
    static thread_local Scheduler * current_;

    std::thread::id thread_id_;

    int epoll_fd_{ -1 };
    int wakeup_fd_{ -1 };
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

    std::unordered_map<int, IoChannelContext> ioChannels_;

    std::unique_ptr<IoChannel> wakeupChannel_;
};

} // namespace my_coro
