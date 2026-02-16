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

#include <nitro_coro/core/CoroTraits.h>
#include <nitro_coro/core/MpscQueue.h>

namespace nitro_coro
{

namespace io
{
class IoChannel;
}

class Scheduler;

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
        uint64_t id;
        int fd;
        std::weak_ptr<io::IoChannel> weakChannel;
        IoEventHandler handler;
        bool addedToEpoll = false;
    };

    Scheduler();
    ~Scheduler();

    Scheduler(const Scheduler &) = delete;
    Scheduler & operator=(const Scheduler &) = delete;

    static Scheduler * current() noexcept;

    void run();
    void stop();

    bool isInOwnThread() const noexcept;

    void setIoChannelHandler(const std::shared_ptr<io::IoChannel> & channel, IoEventHandler handler);
    void updateIoChannel(const std::shared_ptr<io::IoChannel> & channel);
    void removeIoChannel(uint64_t id);

    TimerAwaiter sleep_for(double seconds);
    TimerAwaiter sleep_until(TimePoint when);
    SchedulerAwaiter run_here() noexcept;

    void schedule(std::coroutine_handle<> handle);
    void schedule_at(TimePoint when, std::coroutine_handle<> handle);
    template <typename Func>
    void schedule(Func && func)
    {
        ready_queue_.push(std::forward<Func>(func));
        if (!isInOwnThread())
        {
            wakeup();
        }
    }

    template <typename Func>
    void dispatch(Func && func)
    {
        if (isInOwnThread())
        {
            std::forward<Func>(func)();
        }
        else
        {
            ready_queue_.push(std::forward<Func>(func));
            wakeup();
        }
    }

    template <typename Func>
    void spawn(Func && func)
    {
        static_assert(is_awaitable_v<std::decay_t<decltype(func())>>);

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
                // do not suspend at final, auto destroy
                std::suspend_never final_suspend() const noexcept { return {}; }
            };
        };

        auto task = [](std::decay_t<Func> func) -> FireAndForget {
            co_await func();
            co_return;
        }(std::forward<Func>(func));
        schedule(task.handle_);
    }

private:
    static thread_local Scheduler * current_;

    std::thread::id thread_id_;

    int epoll_fd_{ -1 };
    int wakeup_fd_{ -1 };
    std::atomic<bool> running_{ false };

    MpscQueue<std::function<void()>> ready_queue_;

    struct Timer
    {
        TimePoint when;
        std::coroutine_handle<> handle;

        bool operator>(const Timer & other) const
        {
            return when > other.when;
        }
    };
    std::priority_queue<Timer, std::vector<Timer>, std::greater<>> timers_;
    MpscQueue<Timer> pending_timers_;

    int64_t get_next_timeout();
    void process_ready_queue();
    void process_timers();
    void process_io_events(int timeout_ms);
    void wakeup();

    std::unordered_map<uint64_t, IoChannelContext> ioChannels_;

    std::shared_ptr<io::IoChannel> wakeupChannel_;
};

} // namespace nitro_coro
