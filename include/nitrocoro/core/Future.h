/**
 * @file Future.h
 * @brief Coroutine-aware Promise/Future implementation
 */
#pragma once

#include <nitrocoro/core/LockFreeList.h>
#include <nitrocoro/core/Scheduler.h>

#include <atomic>
#include <coroutine>
#include <exception>
#include <memory>
#include <optional>

namespace nitrocoro
{

template <typename T>
class Promise;

template <typename T>
class SharedFuture;

struct FutureStateBase
{
    struct WaiterNode : LockFreeListNode
    {
        std::coroutine_handle<> handle;
    };

    std::atomic<LockFreeListNode *> waiters_{ nullptr };
    std::exception_ptr exception_;

    static void resumeAll(WaiterNode * head, Scheduler * scheduler) noexcept
    {
        for (auto * n = head; n;)
        {
            auto * next = static_cast<WaiterNode *>(n->next_);
            if (scheduler)
                scheduler->schedule(n->handle);
            else
                n->handle.resume();
            n = next;
        }
    }
};

template <typename T = void>
struct FutureState : FutureStateBase
{
    std::optional<T> value_;
};

template <>
struct FutureState<void> : FutureStateBase
{
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
        FutureStateBase::WaiterNode node_;

        bool await_ready() const noexcept
        {
            return LockFreeListNode::closed(state_->waiters_);
        }

        bool await_suspend(std::coroutine_handle<> h) noexcept
        {
            node_.handle = h;
            return LockFreeListNode::push(state_->waiters_, &node_);
        }

        auto await_resume()
        {
            if (state_->exception_)
                std::rethrow_exception(state_->exception_);
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
        FutureStateBase::WaiterNode node_;

        bool await_ready() const noexcept { return LockFreeListNode::closed(state_->waiters_); }

        bool await_suspend(std::coroutine_handle<> h) noexcept
        {
            node_.handle = h;
            return LockFreeListNode::push(state_->waiters_, &node_);
        }

        const T & await_resume() const
        {
            if (state_->exception_)
                std::rethrow_exception(state_->exception_);
            return *state_->value_;
        }
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
        std::shared_ptr<FutureState<>> state_;
        FutureStateBase::WaiterNode node_;

        bool await_ready() const noexcept { return LockFreeListNode::closed(state_->waiters_); }

        bool await_suspend(std::coroutine_handle<> h) noexcept
        {
            node_.handle = h;
            return LockFreeListNode::push(state_->waiters_, &node_);
        }

        void await_resume() const
        {
            if (state_->exception_)
                std::rethrow_exception(state_->exception_);
        }
    };

    [[nodiscard]] Awaiter get() const noexcept { return Awaiter{ state_ }; }
    bool valid() const noexcept { return state_ != nullptr; }

private:
    friend class Future<>;

    explicit SharedFuture(std::shared_ptr<FutureState<>> state)
        : state_(std::move(state))
    {
    }

    std::shared_ptr<FutureState<>> state_;
};

template <typename T>
SharedFuture<T> Future<T>::share() noexcept
{
    return SharedFuture<T>(std::move(state_));
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
        state_->value_.emplace(std::move(value));
        auto * waiters = static_cast<FutureStateBase::WaiterNode *>(LockFreeListNode::close(state_->waiters_));
        FutureStateBase::resumeAll(waiters, scheduler_);
    }

    void set_exception(std::exception_ptr ex)
    {
        state_->exception_ = std::move(ex);
        auto * waiters = static_cast<FutureStateBase::WaiterNode *>(LockFreeListNode::close(state_->waiters_));
        FutureStateBase::resumeAll(waiters, scheduler_);
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
        , state_(std::make_shared<FutureState<>>())
    {
    }

    Promise(const Promise &) = delete;
    Promise & operator=(const Promise &) = delete;
    Promise(Promise &&) = default;
    Promise & operator=(Promise &&) = default;

    Future<> get_future() { return Future<>(state_); }

    void set_value()
    {
        auto * waiters = static_cast<FutureStateBase::WaiterNode *>(LockFreeListNode::close(state_->waiters_));
        FutureStateBase::resumeAll(waiters, scheduler_);
    }

    void set_exception(std::exception_ptr ex)
    {
        state_->exception_ = std::move(ex);
        auto * waiters = LockFreeListNode::close(state_->waiters_);
        FutureStateBase::resumeAll(static_cast<FutureStateBase::WaiterNode *>(waiters), scheduler_);
    }

private:
    Scheduler * scheduler_;
    std::shared_ptr<FutureState<>> state_;
};

} // namespace nitrocoro
