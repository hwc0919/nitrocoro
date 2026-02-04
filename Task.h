/**
 * @file Task.h
 * @brief coroutine Task declaration
 */
#pragma once

#include <atomic>
#include <chrono>
#include <coroutine>
#include <exception>
#include <queue>
#include <unordered_map>
#include <unordered_set>

namespace my_coro
{

template <typename PromiseType>
struct TaskFinalAwaiter
{
    bool await_ready() noexcept { return false; }
    std::coroutine_handle<> await_suspend(std::coroutine_handle<PromiseType> h) noexcept
    {
        auto & promise = h.promise();
        if (promise.continuation_)
            return promise.continuation_;
        return std::noop_coroutine();
    }
    void await_resume() noexcept {}
};

template <typename T = void>
struct [[nodiscard]] Task
{
    struct promise_type
    {
        std::coroutine_handle<> continuation_;
        T value_;
        std::exception_ptr exception_;

        Task get_return_object() { return Task{ std::coroutine_handle<promise_type>::from_promise(*this) }; }
        std::suspend_always initial_suspend() { return {}; }
        TaskFinalAwaiter<promise_type> final_suspend() noexcept { return {}; }
        void return_value(T value) { value_ = std::move(value); }
        void unhandled_exception() { exception_ = std::current_exception(); }
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
        T await_resume()
        {
            if (handle_.promise().exception_)
                std::rethrow_exception(handle_.promise().exception_);
            return std::move(handle_.promise().value_);
        }
    };

    auto operator co_await() { return Awaiter{ handle_ }; }
};

template <>
struct [[nodiscard]] Task<void>
{
    struct promise_type
    {
        std::coroutine_handle<> continuation_;
        std::exception_ptr exception_;

        Task get_return_object() { return Task{ std::coroutine_handle<promise_type>::from_promise(*this) }; }
        std::suspend_always initial_suspend() { return {}; }
        TaskFinalAwaiter<promise_type> final_suspend() noexcept { return {}; }
        void return_void() {}
        void unhandled_exception() { exception_ = std::current_exception(); }
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
        void await_resume()
        {
            if (handle_.promise().exception_)
                std::rethrow_exception(handle_.promise().exception_);
        }
    };

    auto operator co_await() { return Awaiter{ handle_ }; }
};

} // namespace my_coro
