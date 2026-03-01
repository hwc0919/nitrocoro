/**
 * @file Task.h
 * @brief coroutine Task declaration
 */
#pragma once

#include <coroutine>
#include <exception>
#include <optional>

namespace nitrocoro
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

template <typename PromiseType>
struct TaskAwaiter
{
    std::coroutine_handle<PromiseType> handle_;

    explicit TaskAwaiter(std::coroutine_handle<PromiseType> h) noexcept
        : handle_(h) {}

    // Check done() so that if the Task temporary is destroyed before co_await
    // resumes (e.g. a prvalue Task returned from a function), the coroutine
    // frame is already at its final-suspend point and ~Task() can safely
    // destroy it without racing with the symmetric-transfer resume.
    bool await_ready() noexcept { return !handle_ || handle_.done(); }
    std::coroutine_handle<> await_suspend(std::coroutine_handle<> h)
    {
        handle_.promise().continuation_ = h;
        return handle_;
    }

    auto await_resume()
    {
        if constexpr (std::is_void_v<decltype(handle_.promise().result())>)
        {
            handle_.promise().result();
            return;
        }
        else
        {
            return std::move(handle_.promise().result());
        }
    }
};

template <typename T = void>
struct [[nodiscard]] Task
{
    struct promise_type;
    using handle_type = std::coroutine_handle<promise_type>;

    struct promise_type
    {
        std::coroutine_handle<> continuation_;
        std::optional<T> value_;
        std::exception_ptr exception_;

        Task get_return_object() { return Task{ handle_type::from_promise(*this) }; }
        std::suspend_always initial_suspend() { return {}; }
        TaskFinalAwaiter<promise_type> final_suspend() noexcept { return {}; }
        void return_value(const T & value) { value_ = value; }
        void return_value(T && value) { value_ = std::move(value); }
        void unhandled_exception() { exception_ = std::current_exception(); }
        T && result() &&
        {
            if (exception_)
                std::rethrow_exception(exception_);
            return std::move(value_.value());
        }
        T & result() &
        {
            if (exception_)
                std::rethrow_exception(exception_);
            return value_.value();
        }
    };

    handle_type handle_;

    Task(handle_type h)
        : handle_(h) {}
    Task(Task && other) noexcept
        : handle_(other.handle_) { other.handle_ = nullptr; }
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

    auto operator co_await() const noexcept
    {
        return TaskAwaiter{ handle_ };
    }
};

template <>
struct [[nodiscard]] Task<void>
{
    struct promise_type;
    using handle_type = std::coroutine_handle<promise_type>;

    struct promise_type
    {
        std::coroutine_handle<> continuation_;
        std::exception_ptr exception_;

        Task get_return_object() { return Task{ handle_type::from_promise(*this) }; }
        std::suspend_always initial_suspend() { return {}; }
        TaskFinalAwaiter<promise_type> final_suspend() noexcept { return {}; }
        void return_void() {}
        void unhandled_exception() { exception_ = std::current_exception(); }
        void result() const
        {
            if (exception_)
                std::rethrow_exception(exception_);
        }
    };

    handle_type handle_;

    Task(handle_type h)
        : handle_(h) {}
    Task(Task && other) noexcept
        : handle_(other.handle_) { other.handle_ = nullptr; }
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

    auto operator co_await() const noexcept
    {
        return TaskAwaiter{ handle_ };
    }
};

} // namespace nitrocoro
