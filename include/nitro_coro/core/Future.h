/**
 * @file Future.h
 * @brief Coroutine-aware Promise/Future implementation
 */
#pragma once

#include <nitro_coro/core/Scheduler.h>

#include <coroutine>
#include <memory>
#include <mutex>
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
    std::mutex mutex_;
    bool ready_{ false };
    std::vector<std::coroutine_handle<>> waiters_;
    std::optional<T> value_;
};

template <>
struct FutureState<void>
{
    std::mutex mutex_;
    bool ready_{ false };
    std::vector<std::coroutine_handle<>> waiters_;
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

        bool await_ready() const noexcept
        {
            std::lock_guard lock(state_->mutex_);
            return state_->ready_;
        }

        bool await_suspend(std::coroutine_handle<> h) noexcept
        {
            std::lock_guard lock(state_->mutex_);
            if (state_->ready_)
            {
                return false;
            }
            state_->waiters_.push_back(h);
            return true;
        }

        T await_resume() noexcept
        {
            if constexpr (!std::is_void_v<T>)
                return std::move(*state_->value_);
        }
    };

    [[nodiscard]] Awaiter get() noexcept
    {
        auto awaiter = Awaiter{ state_ };
        state_.reset();
        return awaiter;
    }

    bool valid() const noexcept { return state_ != nullptr; }
    SharedFuture<T> share() noexcept;

private:
    friend class Promise<T>;

    explicit Future(std::shared_ptr<FutureState<T>> state)
        : state_(std::move(state))
    {
    }

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

        bool await_ready() const noexcept
        {
            std::lock_guard lock(state_->mutex_);
            return state_->ready_;
        }

        bool await_suspend(std::coroutine_handle<> h) noexcept
        {
            std::lock_guard lock(state_->mutex_);
            if (state_->ready_)
            {
                return false;
            }
            state_->waiters_.push_back(h);
            return true;
        }

        void await_resume() noexcept
        {
        }
    };

    [[nodiscard]] Awaiter get() noexcept
    {
        auto awaiter = Awaiter{ state_ };
        state_.reset();
        return awaiter;
    }

    bool valid() const noexcept { return state_ != nullptr; }
    SharedFuture<void> share() noexcept;

private:
    friend class Promise<void>;

    explicit Future(std::shared_ptr<FutureState<void>> state)
        : state_(std::move(state))
    {
    }

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

        bool await_ready() const noexcept
        {
            std::lock_guard lock(state_->mutex_);
            return state_->ready_;
        }

        bool await_suspend(std::coroutine_handle<> h) noexcept
        {
            std::lock_guard lock(state_->mutex_);
            if (state_->ready_)
            {
                return false;
            }
            state_->waiters_.push_back(h);
            return true;
        }

        const T & await_resume() const noexcept { return *state_->value_; }
    };

    [[nodiscard]] Awaiter get() const noexcept { return Awaiter{ state_ }; }
    bool valid() const noexcept { return state_ != nullptr; }

private:
    friend class Future<T>;

    explicit SharedFuture(std::shared_ptr<FutureState<T>> state)
        : state_(std::move(state))
    {
    }

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

        bool await_ready() const noexcept
        {
            std::lock_guard lock(state_->mutex_);
            return state_->ready_;
        }

        bool await_suspend(std::coroutine_handle<> h) noexcept
        {
            std::lock_guard lock(state_->mutex_);
            if (state_->ready_)
            {
                return false;
            }
            state_->waiters_.push_back(h);
            return true;
        }

        void await_resume() const noexcept
        {
        }
    };

    [[nodiscard]] Awaiter get() const noexcept { return Awaiter{ state_ }; }
    bool valid() const noexcept { return state_ != nullptr; }

private:
    friend class Future<void>;

    explicit SharedFuture(std::shared_ptr<FutureState<void>> state)
        : state_(std::move(state))
    {
    }

    std::shared_ptr<FutureState<void>> state_;
};

template <typename T>
SharedFuture<T> Future<T>::share() noexcept
{
    return SharedFuture<T>(std::move(state_));
}

inline SharedFuture<void> Future<void>::share() noexcept
{
    return SharedFuture<void>(std::move(state_));
}

template <typename T = void>
class Promise
{
public:
    explicit Promise(Scheduler * scheduler)
        : scheduler_(scheduler)
        , state_(std::make_shared<FutureState<T>>())
    {
    }

    Promise(const Promise &) = delete;
    Promise & operator=(const Promise &) = delete;
    Promise(Promise &&) = default;
    Promise & operator=(Promise &&) = default;

    Future<T> get_future() { return Future<T>(state_); }

    void set_value(T value)
    {
        std::vector<std::coroutine_handle<>> waiters;
        {
            std::lock_guard lock(state_->mutex_);
            state_->value_.emplace(std::move(value));
            state_->ready_ = true;
            waiters = std::move(state_->waiters_);
        }
        for (auto h : waiters)
        {
            scheduler_->schedule(h);
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
    explicit Promise(Scheduler * scheduler)
        : scheduler_(scheduler)
        , state_(std::make_shared<FutureState<void>>())
    {
    }

    Promise(const Promise &) = delete;
    Promise & operator=(const Promise &) = delete;
    Promise(Promise &&) = default;
    Promise & operator=(Promise &&) = default;

    Future<void> get_future() { return Future<void>(state_); }

    void set_value()
    {
        std::vector<std::coroutine_handle<>> waiters;
        {
            std::lock_guard lock(state_->mutex_);
            state_->ready_ = true;
            waiters = std::move(state_->waiters_);
        }
        for (auto h : waiters)
        {
            scheduler_->schedule(h);
        }
    }

private:
    Scheduler * scheduler_;
    std::shared_ptr<FutureState<void>> state_;
};

} // namespace nitro_coro
