/**
 * @file CoroScheduler.h
 * @brief Native coroutine scheduler - replaces EventLoop for coroutine-first design
 */
#pragma once

#include <atomic>
#include <chrono>
#include <coroutine>
#include <queue>
#include <unordered_map>

namespace drogon::coro
{

enum class IoOp
{
    Read,
    Write
};

using TimerId = uint64_t;
using TimePoint = std::chrono::steady_clock::time_point;

struct IoAwaitable
{
    int fd_;
    void * buf_;
    size_t len_;
    IoOp op_;
    std::coroutine_handle<> handle_;
    ssize_t result_{-1};

    bool await_ready() const noexcept { return false; }
    void await_suspend(std::coroutine_handle<> h) noexcept;
    ssize_t await_resume() noexcept { return result_; }
};

struct TimerAwaitable
{
    TimePoint when_;
    std::coroutine_handle<> handle_;

    bool await_ready() const noexcept { return false; }
    void await_suspend(std::coroutine_handle<> h) noexcept;
    void await_resume() noexcept {}
};

class CoroScheduler
{
public:
    CoroScheduler();
    ~CoroScheduler();

    void run();
    void stop();

    // 协程感知的 I/O
    IoAwaitable async_read(int fd, void * buf, size_t len);
    IoAwaitable async_write(int fd, const void * buf, size_t len);

    // 协程感知的定时器
    TimerAwaitable sleep_for(double seconds);
    TimerAwaitable sleep_until(TimePoint when);

    void schedule(std::coroutine_handle<> coro);
    void register_io(int fd, IoOp op, std::coroutine_handle<> coro, void * buf, size_t len);
    TimerId register_timer(TimePoint when, std::coroutine_handle<> coro);

private:
    int epoll_fd_;
    std::atomic<bool> running_{false};

    struct ReadyQueue
    {
        std::coroutine_handle<> coros[1024];
        std::atomic<size_t> head_{0};
        std::atomic<size_t> tail_{0};

        void push(std::coroutine_handle<> h);
        std::coroutine_handle<> pop();
    } ready_queue_;

    struct IoWaiter
    {
        std::coroutine_handle<> coro;
        void * buffer;
        size_t size;
    };
    std::unordered_map<int, IoWaiter> io_waiters_;

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
    std::atomic<TimerId> next_timer_id_{1};

    void process_io_events();
    void process_timers();
    void resume_ready_coros();
    int64_t get_next_timeout() const;
};

extern thread_local CoroScheduler * g_scheduler;

inline CoroScheduler * current_scheduler() { return g_scheduler; }

} // namespace drogon::coro
