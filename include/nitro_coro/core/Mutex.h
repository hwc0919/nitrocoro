/**
 * @file Mutex.h
 * @brief Coroutine-aware mutex implementation
 *
 * This implementation is adapted from the Drogon framework:
 * https://github.com/drogonframework/drogon/pull/2095
 * Original author: fantasy-peak (https://github.com/fantasy-peak)
 *
 * A lock-free mutex designed for C++20 coroutines that suspends waiting
 * coroutines instead of blocking threads.
 */

#pragma once
#include <atomic>
#include <cassert>
#include <coroutine>
#include <mutex>
#include <nitro_coro/core/Scheduler.h>

namespace nitro_coro
{

class Mutex final
{
    class ScopedCoroMutexAwaiter;
    class CoroMutexAwaiter;

public:
    Mutex() noexcept : state_(unlockedValue()), waiters_(nullptr)
    {
    }

    Mutex(const Mutex &) = delete;
    Mutex(Mutex &&) = delete;
    Mutex & operator=(const Mutex &) = delete;
    Mutex & operator=(Mutex &&) = delete;

    ~Mutex()
    {
        [[maybe_unused]] auto state = state_.load(std::memory_order_relaxed);
        assert(state == unlockedValue() || state == nullptr);
        assert(waiters_ == nullptr);
    }

    bool try_lock() noexcept
    {
        void * oldValue = unlockedValue();
        return state_.compare_exchange_strong(oldValue,
                                              nullptr,
                                              std::memory_order_acquire,
                                              std::memory_order_relaxed);
    }

    [[nodiscard]] ScopedCoroMutexAwaiter scoped_lock(
        Scheduler * sched = Scheduler::current()) noexcept
    {
        return ScopedCoroMutexAwaiter(*this, sched);
    }

    [[nodiscard]] CoroMutexAwaiter lock(
        Scheduler * sched = Scheduler::current()) noexcept
    {
        return CoroMutexAwaiter(*this, sched);
    }

    void unlock() noexcept
    {
        assert(state_.load(std::memory_order_relaxed) != unlockedValue());
        auto * waitersHead = waiters_;
        if (waitersHead == nullptr)
        {
            void * currentState = state_.load(std::memory_order_relaxed);
            if (currentState == nullptr)
            {
                const bool releasedLock = state_.compare_exchange_strong(currentState,
                                                                         unlockedValue(),
                                                                         std::memory_order_release,
                                                                         std::memory_order_relaxed);
                if (releasedLock)
                {
                    return;
                }
            }
            currentState = state_.exchange(nullptr, std::memory_order_acquire);
            assert(currentState != unlockedValue());
            assert(currentState != nullptr);
            auto * waiter = static_cast<CoroMutexAwaiter *>(currentState);
            do
            {
                auto * temp = waiter->next_;
                waiter->next_ = waitersHead;
                waitersHead = waiter;
                waiter = temp;
            } while (waiter != nullptr);
        }
        assert(waitersHead != nullptr);
        waiters_ = waitersHead->next_;
        if (waitersHead->sched_)
        {
            auto handle = waitersHead->handle_;
            waitersHead->sched_->schedule(handle);
        }
        else
        {
            waitersHead->handle_.resume();
        }
    }

private:
    class CoroMutexAwaiter
    {
    public:
        CoroMutexAwaiter(Mutex & mutex, Scheduler * sched) noexcept
            : mutex_(mutex), sched_(sched)
        {
        }

        bool await_ready() noexcept
        {
            return mutex_.try_lock();
        }

        bool await_suspend(std::coroutine_handle<> handle) noexcept
        {
            handle_ = handle;
            return mutex_.asynclockImpl(this);
        }

        void await_resume() noexcept
        {
        }

    private:
        friend class Mutex;

        Mutex & mutex_;
        Scheduler * sched_;
        std::coroutine_handle<> handle_;
        CoroMutexAwaiter * next_;
    };

    class ScopedCoroMutexAwaiter : public CoroMutexAwaiter
    {
    public:
        ScopedCoroMutexAwaiter(Mutex & mutex, Scheduler * sched)
            : CoroMutexAwaiter(mutex, sched)
        {
        }

        [[nodiscard]] auto await_resume() noexcept
        {
            return std::unique_lock<Mutex>{ mutex_, std::adopt_lock };
        }
    };

    bool asynclockImpl(CoroMutexAwaiter * awaiter)
    {
        void * oldValue = state_.load(std::memory_order_relaxed);
        while (true)
        {
            if (oldValue == unlockedValue())
            {
                void * newValue = nullptr;
                if (state_.compare_exchange_weak(oldValue,
                                                 newValue,
                                                 std::memory_order_acquire,
                                                 std::memory_order_relaxed))
                {
                    return false;
                }
            }
            else
            {
                void * newValue = awaiter;
                awaiter->next_ = static_cast<CoroMutexAwaiter *>(oldValue);
                if (state_.compare_exchange_weak(oldValue,
                                                 newValue,
                                                 std::memory_order_release,
                                                 std::memory_order_relaxed))
                {
                    return true;
                }
            }
        }
    }

    void * unlockedValue() noexcept
    {
        return this;
    }

    std::atomic<void *> state_;
    CoroMutexAwaiter * waiters_;
};

} // namespace nitro_coro
