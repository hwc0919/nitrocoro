/**
 * @file Future.h
 * @brief Coroutine-aware Promise/Future implementation
 */
#pragma once

#include <coroutine>
#include <memory>
#include <nitro_coro/core/Scheduler.h>
#include <optional>

namespace nitro_coro
{

template <typename T>
class Promise;

template <typename T = void>
struct FutureState
{
    bool ready_{ false };
    std::coroutine_handle<> waiter_;
    std::optional<T> value_;
};

template <>
struct FutureState<void>
{
    bool ready_{ false };
    std::coroutine_handle<> waiter_;
};

template <typename T = void>
class Future
{
public:
    Future(const Future &) = delete;
    Future & operator=(const Future &) = delete;
    Future(Future &&) = default;
    Future & operator=(Future &&) = default;

    struct Awaiter
    {
        std::shared_ptr<FutureState<T>> state_;
        bool await_ready() const noexcept { return state_->ready_; }
        void await_suspend(std::coroutine_handle<> h) noexcept { state_->waiter_ = h; }
        T await_resume() noexcept
        {
            if constexpr (!std::is_void_v<T>)
                return std::move(*state_->value_);
        }
    };

    Awaiter get() noexcept { return Awaiter{ state_ }; }
    bool valid() const noexcept { return state_ != nullptr; }

private:
    friend class Promise<T>;

    explicit Future(std::shared_ptr<FutureState<T>> state) : state_(std::move(state)) {}

    std::shared_ptr<FutureState<T>> state_;
};

template <>
class Future<void>
{
public:
    Future(const Future &) = delete;
    Future & operator=(const Future &) = delete;
    Future(Future &&) = default;
    Future & operator=(Future &&) = default;

    struct Awaiter
    {
        std::shared_ptr<FutureState<void>> state_;
        bool await_ready() const noexcept { return state_->ready_; }
        void await_suspend(std::coroutine_handle<> h) noexcept { state_->waiter_ = h; }
        void await_resume() noexcept {}
    };

    Awaiter get() noexcept { return Awaiter{ state_ }; }
    bool valid() const noexcept { return state_ != nullptr; }

private:
    friend class Promise<void>;

    explicit Future(std::shared_ptr<FutureState<void>> state) : state_(std::move(state)) {}

    std::shared_ptr<FutureState<void>> state_;
};

template <typename T = void>
class Promise
{
public:
    explicit Promise(Scheduler * scheduler) : scheduler_(scheduler), state_(std::make_shared<FutureState<T>>()) {}

    Promise(const Promise &) = delete;
    Promise & operator=(const Promise &) = delete;
    Promise(Promise &&) = default;
    Promise & operator=(Promise &&) = default;

    Future<T> get_future() { return Future<T>(state_); }

    void set_value(T value)
    {
        state_->value_.emplace(std::move(value));
        state_->ready_ = true;
        if (state_->waiter_)
        {
            scheduler_->schedule(state_->waiter_);
            state_->waiter_ = nullptr;
        }
    }

private:
    Scheduler * scheduler_;
    std::shared_ptr<FutureState<T>> state_;
};

template <>
class Promise<void>
{
public:
    explicit Promise(Scheduler * scheduler) : scheduler_(scheduler), state_(std::make_shared<FutureState<void>>()) {}

    Promise(const Promise &) = delete;
    Promise & operator=(const Promise &) = delete;
    Promise(Promise &&) = default;
    Promise & operator=(Promise &&) = default;

    Future<void> get_future() { return Future<void>(state_); }

    void set_value()
    {
        state_->ready_ = true;
        if (state_->waiter_)
        {
            scheduler_->schedule(state_->waiter_);
            state_->waiter_ = nullptr;
        }
    }

private:
    Scheduler * scheduler_;
    std::shared_ptr<FutureState<void>> state_;
};

} // namespace nitro_coro
