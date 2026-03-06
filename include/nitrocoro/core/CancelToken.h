/**
 * @file CancelToken.h
 * @brief CancelSource / CancelToken — cooperative cancellation for coroutines.
 */
#pragma once

#include <nitrocoro/core/LockFreeList.h>
#include <nitrocoro/core/Scheduler.h>

#include <atomic>
#include <coroutine>
#include <functional>
#include <memory>
#include <mutex>
#include <vector>

namespace nitrocoro
{

class CancelToken;
class CancelRegistration;

namespace detail
{

struct CancelState
{
    struct WaiterNode : LockFreeListNode
    {
        std::coroutine_handle<> handle;
        Scheduler * sched;
    };

    std::atomic<LockFreeListNode *> waiters_{ nullptr };

    // callbacks protected by mutex (cancel() may be called from any thread)
    std::mutex cbMutex_;
    std::vector<std::pair<uint64_t, std::function<void()>>> callbacks_;
    uint64_t nextId_{ 0 };

    bool isCancelled() const noexcept
    {
        return LockFreeListNode::closed(waiters_);
    }

    void cancel()
    {
        auto * head = static_cast<WaiterNode *>(LockFreeListNode::close(waiters_));
        if (head == reinterpret_cast<WaiterNode *>(LockFreeListNode::kClosed))
            return; // already cancelled

        // fire callbacks
        decltype(callbacks_) cbs;
        {
            std::lock_guard lock(cbMutex_);
            cbs.swap(callbacks_);
        }
        for (auto & [id, cb] : cbs)
            cb();

        // resume waiters
        for (auto * n = head; n;)
        {
            auto * next = static_cast<WaiterNode *>(n->next_);
            if (n->sched)
                n->sched->schedule(n->handle);
            else
                n->handle.resume();
            n = next;
        }
    }

    uint64_t addCallback(std::function<void()> cb)
    {
        std::lock_guard lock(cbMutex_);
        if (isCancelled())
        {
            cb();
            return 0;
        }
        uint64_t id = ++nextId_;
        callbacks_.emplace_back(id, std::move(cb));
        return id;
    }

    void removeCallback(uint64_t id)
    {
        if (id == 0)
            return;
        std::lock_guard lock(cbMutex_);
        auto it = std::find_if(callbacks_.begin(), callbacks_.end(), [id](auto & p) { return p.first == id; });
        if (it != callbacks_.end())
            callbacks_.erase(it);
    }
};

} // namespace detail

// ─── CancelRegistration ──────────────────────────────────────────────────────

class CancelRegistration
{
public:
    CancelRegistration() = default;

    CancelRegistration(std::shared_ptr<detail::CancelState> state, uint64_t id)
        : state_(std::move(state)), id_(id)
    {
    }

    ~CancelRegistration()
    {
        if (state_)
            state_->removeCallback(id_);
    }

    CancelRegistration(CancelRegistration &&) noexcept = default;
    CancelRegistration & operator=(CancelRegistration &&) noexcept = default;
    CancelRegistration(const CancelRegistration &) = delete;
    CancelRegistration & operator=(const CancelRegistration &) = delete;

private:
    std::shared_ptr<detail::CancelState> state_;
    uint64_t id_{ 0 };
};

// ─── CancelToken ─────────────────────────────────────────────────────────────

class CancelToken
{
public:
    CancelToken() = default; // None — never cancelled

    bool isCancelled() const noexcept
    {
        return state_ && state_->isCancelled();
    }

    [[nodiscard]] CancelRegistration onCancel(std::function<void()> cb)
    {
        if (!state_)
            return {};
        uint64_t id = state_->addCallback(std::move(cb));
        return CancelRegistration(state_, id);
    }

    struct [[nodiscard]] CancelledAwaiter
    {
        std::shared_ptr<detail::CancelState> state_;
        detail::CancelState::WaiterNode node_;

        bool await_ready() const noexcept
        {
            return !state_ || LockFreeListNode::closed(state_->waiters_);
        }

        bool await_suspend(std::coroutine_handle<> h) noexcept
        {
            node_.handle = h;
            node_.sched = Scheduler::current();
            return LockFreeListNode::push(state_->waiters_, &node_);
        }

        void await_resume() noexcept {}
    };

    [[nodiscard]] CancelledAwaiter cancelled()
    {
        return CancelledAwaiter{ state_ };
    }

private:
    friend class CancelSource;

    explicit CancelToken(std::shared_ptr<detail::CancelState> state)
        : state_(std::move(state))
    {
    }

    std::shared_ptr<detail::CancelState> state_;
};

// ─── CancelSource ─────────────────────────────────────────────────────────────

class CancelSource
{
public:
    CancelSource()
        : state_(std::make_shared<detail::CancelState>())
    {
    }

    ~CancelSource() { cancel(); }

    CancelToken token() const { return CancelToken(state_); }

    void cancel() { state_->cancel(); }

    bool isCancelled() const noexcept { return state_->isCancelled(); }

private:
    std::shared_ptr<detail::CancelState> state_;
};

} // namespace nitrocoro
