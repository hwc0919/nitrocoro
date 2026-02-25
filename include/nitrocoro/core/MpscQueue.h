/**
 * @file MpscQueue.h
 * @brief Lock-free Multiple Producer Single Consumer Queue (unbounded)
 */
#pragma once

#include <atomic>
#include <optional>
#include <utility>

namespace nitrocoro
{

template <typename T>
class MpscQueue
{
public:
    MpscQueue()
    {
        Node * stub = new Node();
        head_.store(stub, std::memory_order_relaxed);
        tail_ = stub;
    }

    ~MpscQueue()
    {
        while (Node * node = head_.load(std::memory_order_relaxed))
        {
            head_.store(node->next.load(std::memory_order_relaxed), std::memory_order_relaxed);
            delete node;
        }
    }

    void push(T value)
    {
        Node * node = new Node(std::move(value));
        Node * prev = tail_.exchange(node, std::memory_order_acq_rel);
        prev->next.store(node, std::memory_order_release);
    }

    std::optional<T> pop()
    {
        Node * head = head_.load(std::memory_order_relaxed);
        Node * next = head->next.load(std::memory_order_acquire);

        if (!next)
        {
            return std::nullopt;
        }

        T value = std::move(next->value);
        head_.store(next, std::memory_order_release);
        delete head;
        return value;
    }

    bool empty() const
    {
        Node * head = head_.load(std::memory_order_relaxed);
        return !head->next.load(std::memory_order_acquire);
    }

private:
    struct Node
    {
        T value;
        std::atomic<Node *> next;

        Node()
            : value{}, next(nullptr)
        {
        }
        explicit Node(T val)
            : value(std::move(val)), next(nullptr)
        {
        }
    };

    /* alignas(64) */ std::atomic<Node *> head_;
    /* alignas(64) */ std::atomic<Node *> tail_;
};

} // namespace nitrocoro
