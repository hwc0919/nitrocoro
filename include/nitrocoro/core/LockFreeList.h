/**
 * @file LockFreeList.h
 * @brief Lock-free one-shot intrusive list node for coroutine synchronization primitives.
 *
 * Usage:
 *   struct MyNode : LockFreeListNode { ... };
 *
 *   std::atomic<LockFreeListNode *> head{ nullptr };
 *   LockFreeListNode::push(head, node)  → false if already closed
 *   LockFreeListNode::close(head)       → returns old head; marks list as closed
 *   LockFreeListNode::closed(head)      → true if closed
 */
#pragma once

#include <atomic>
#include <cstdint>

namespace nitrocoro
{

struct LockFreeListNode
{
    LockFreeListNode * next_{ nullptr };

    static inline LockFreeListNode * const kClosed = reinterpret_cast<LockFreeListNode *>(~uintptr_t{ 0 });

    static bool push(std::atomic<LockFreeListNode *> & head, LockFreeListNode * node) noexcept
    {
        LockFreeListNode * cur = head.load(std::memory_order_acquire);
        do
        {
            if (cur == kClosed)
                return false;
            node->next_ = cur;
        } while (!head.compare_exchange_weak(cur, node, std::memory_order_release, std::memory_order_acquire));
        return true;
    }

    static LockFreeListNode * close(std::atomic<LockFreeListNode *> & head) noexcept
    {
        return head.exchange(kClosed, std::memory_order_acq_rel);
    }

    static bool closed(const std::atomic<LockFreeListNode *> & head) noexcept
    {
        return head.load(std::memory_order_acquire) == kClosed;
    }
};

} // namespace nitrocoro
