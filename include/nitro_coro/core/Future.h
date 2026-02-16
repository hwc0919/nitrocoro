/**
 * @file Future.h
 * @brief Coroutine-aware Promise/Future implementation
 */
#pragma once

#include <coroutine>
#include <memory>
#include <nitro_coro/core/Scheduler.h>
#include <optional>
#include <vector>

namespace nitro_coro
{

template <typename T>
class Promise;

template <typename T>
class SharedFuture;

template <typename T = void>
struct FutureState
{
    bool ready_{ false };
    std::vector<std::coroutine_handle<>> waiters_; // 支持多等待者
    std::optional<T> value_;
};

template <>
struct FutureState<void>
{
    bool ready_{ false };
    std::vector<std::coroutine_handle<>> waiters_; // 支持多等待者
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
        void await_suspend(std::coroutine_handle<> h) noexcept { state_->waiters_.push_back(h); }
        T await_resume() noexcept
        {
            if constexpr (!std::is_void_v<T>)
                return std::move(*state_->value_);
        }
    };

    [[nodiscard]] Awaiter get() noexcept
    {
        auto awaiter = Awaiter{ state_ };
        state_.reset(); // 重置状态，只能调用一次
        return awaiter;
    }
    bool valid() const noexcept { return state_ != nullptr; }
    SharedFuture<T> share() noexcept;

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
        void await_suspend(std::coroutine_handle<> h) noexcept { state_->waiters_.push_back(h); }
        void await_resume() noexcept {}
    };

    [[nodiscard]] Awaiter get() noexcept
    {
        auto awaiter = Awaiter{ state_ };
        state_.reset(); // 重置状态，只能调用一次
        return awaiter;
    }
    bool valid() const noexcept { return state_ != nullptr; }
    SharedFuture<void> share() noexcept;

private:
    friend class Promise<void>;

    explicit Future(std::shared_ptr<FutureState<void>> state) : state_(std::move(state)) {}

    std::shared_ptr<FutureState<void>> state_;
};

template <typename T = void>
class SharedFuture
{
public:
    SharedFuture(const SharedFuture &) = default;
    SharedFuture & operator=(const SharedFuture &) = default;
    SharedFuture(SharedFuture &&) = default;
    SharedFuture & operator=(SharedFuture &&) = default;

    struct Awaiter
    {
        std::shared_ptr<FutureState<T>> state_;
        bool await_ready() const noexcept { return state_->ready_; }
        void await_suspend(std::coroutine_handle<> h) noexcept { state_->waiters_.push_back(h); }
        const T & await_resume() const noexcept { return *state_->value_; }
    };

    [[nodiscard]] Awaiter get() const noexcept { return Awaiter{ state_ }; }
    bool valid() const noexcept { return state_ != nullptr; }

private:
    friend class Future<T>;

    explicit SharedFuture(std::shared_ptr<FutureState<T>> state) : state_(std::move(state)) {}

    std::shared_ptr<FutureState<T>> state_;
};

template <>
class SharedFuture<void>
{
public:
    SharedFuture(const SharedFuture &) = default;
    SharedFuture & operator=(const SharedFuture &) = default;
    SharedFuture(SharedFuture &&) = default;
    SharedFuture & operator=(SharedFuture &&) = default;

    struct Awaiter
    {
        std::shared_ptr<FutureState<void>> state_;
        bool await_ready() const noexcept { return state_->ready_; }
        void await_suspend(std::coroutine_handle<> h) noexcept { state_->waiters_.push_back(h); }
        void await_resume() const noexcept {}
    };

    [[nodiscard]] Awaiter get() const noexcept { return Awaiter{ state_ }; }
    bool valid() const noexcept { return state_ != nullptr; }

private:
    friend class Future<void>;

    explicit SharedFuture(std::shared_ptr<FutureState<void>> state) : state_(std::move(state)) {}

    std::shared_ptr<FutureState<void>> state_;
};

template <typename T>
SharedFuture<T> Future<T>::share() noexcept
{
    return SharedFuture<T>(std::move(state_)); // 直接转移状态
}

inline SharedFuture<void> Future<void>::share() noexcept
{
    return SharedFuture<void>(std::move(state_)); // 直接转移状态
}

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
        for (auto h : state_->waiters_)
        {
            scheduler_->schedule(h);
        }
        state_->waiters_.clear();
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
        for (auto h : state_->waiters_)
        {
            scheduler_->schedule(h);
        }
        state_->waiters_.clear();
    }

private:
    Scheduler * scheduler_;
    std::shared_ptr<FutureState<void>> state_;
};

} // namespace nitro_coro
