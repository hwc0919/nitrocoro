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

namespace my_coro
{
class Scheduler;
class IoChannel;

enum class IoOp
{
    Read,
    Write
};

using TimerId = uint64_t;
using TimePoint = std::chrono::steady_clock::time_point;

struct [[nodiscard]] TimerAwaitable
{
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

    // 协程感知的定时器
    TimerAwaitable sleep_for(double seconds);
    TimerAwaitable sleep_until(TimePoint when);

    void schedule(std::coroutine_handle<> coro);

    template <typename Coro>
    void spawn(Coro && coro)
    {
        using CoroValueType = std::decay_t<Coro>;
        auto functor = [](CoroValueType coro) -> AsyncTask {
            auto frame = coro();

            using FrameType = std::decay_t<decltype(frame)>;
            // static_assert(is_awaitable_v<FrameType>);

            co_await frame;
            co_return;
        };
        auto task = functor(std::forward<Coro>(coro));
        if (task.coro_)
            schedule(task.coro_);
    }

    TimerId register_timer(TimePoint when, std::coroutine_handle<> coro);

private:
    static thread_local Scheduler * current_;

    int epoll_fd_{ -1 };
    int wakeup_fd_{ -1 }; // eventfd 用于唤醒 epoll
    std::atomic<bool> running_{ false };

    struct ReadyQueue
    {
        std::coroutine_handle<> coros[1024];
        std::atomic<size_t> head_{ 0 };
        std::atomic<size_t> tail_{ 0 };

        void push(std::coroutine_handle<> h);
        std::coroutine_handle<> pop();
    } ready_queue_;

    struct Timer
    {
        TimerId id;
        TimePoint when;
        std::coroutine_handle<> coro;

        bool operator>(const Timer & other) const
        {
            return when > other.when;
        }
    };
    std::priority_queue<Timer, std::vector<Timer>, std::greater<Timer>> timers_;
    std::atomic<TimerId> next_timer_id_{ 1 };

    void resume_ready_coros();
    void process_io_events();
    void process_timers();
    int64_t get_next_timeout() const;
    void wakeup();

    std::unordered_map<int, IoChannel *> ioChannels_;

    std::unique_ptr<IoChannel> wakeupChannel_;
};

} // namespace my_coro
