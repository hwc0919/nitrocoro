/**
 * @file Task.h
 * @brief coroutine Task declaration
 */
#pragma once

#include <atomic>
#include <chrono>
#include <coroutine>
#include <queue>
#include <unordered_map>
#include <unordered_set>

namespace my_coro
{

class CoroScheduler;

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
        AsyncTask get_return_object() noexcept
        {
            return { std::coroutine_handle<promise_type>::from_promise(*this) };
        }

        std::suspend_never initial_suspend() const noexcept
        {
            return {};
        }

        void unhandled_exception()
        {
            std::terminate();
        }

        void return_void() noexcept
        {
        }

        std::suspend_never final_suspend() const noexcept
        {
            return {};
        }
    };

    handle_type coro_;
};

struct [[nodiscard]] Task
{
    struct promise_type
    {
        std::coroutine_handle<> continuation_;

        Task get_return_object() { return Task{ std::coroutine_handle<promise_type>::from_promise(*this) }; }
        std::suspend_always initial_suspend() { return {}; }

        struct FinalAwaiter
        {
            bool await_ready() noexcept { return false; }
            std::coroutine_handle<> await_suspend(std::coroutine_handle<promise_type> h) noexcept
            {
                auto & promise = h.promise();
                if (promise.continuation_)
                    return promise.continuation_;
                return std::noop_coroutine();
            }
            void await_resume() noexcept {}
        };

        FinalAwaiter final_suspend() noexcept { return {}; }
        void return_void() {}
        void unhandled_exception() {}
    };

    std::coroutine_handle<promise_type> handle_;

    Task(std::coroutine_handle<promise_type> h) : handle_(h) {}
    Task(Task && other) noexcept : handle_(other.handle_) { other.handle_ = nullptr; }
    Task & operator=(Task && other) noexcept
    {
        if (this != &other)
        {
            if (handle_)
                handle_.destroy();
            handle_ = other.handle_;
            other.handle_ = nullptr;
        }
        return *this;
    }
    Task(const Task &) = delete;
    Task & operator=(const Task &) = delete;

    ~Task()
    {
        if (handle_)
            handle_.destroy();
    }

    struct Awaiter
    {
        std::coroutine_handle<promise_type> handle_;

        bool await_ready() { return false; }
        std::coroutine_handle<> await_suspend(std::coroutine_handle<> h)
        {
            handle_.promise().continuation_ = h;
            return handle_;
        }
        void await_resume() {}
    };

    auto operator co_await() { return Awaiter{ handle_ }; }

    friend class my_coro::CoroScheduler;
};

} // namespace my_coro
