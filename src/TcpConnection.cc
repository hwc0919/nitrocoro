/**
 * @file TcpConnection.cc
 * @brief Implementation of TcpConnection
 */
#include "TcpConnection.h"
#include "Scheduler.h"
#include <iostream>
#include <unistd.h>

namespace my_coro
{

TcpConnection::TcpConnection(int fd)
    : fd_(fd)
    , ioChannelPtr_(new IoChannel(fd, Scheduler::current()))
{
}

TcpConnection::~TcpConnection() = default;

Task<ssize_t> TcpConnection::read(void * buf, size_t len)
{
    BufferReader reader(buf, len);
    co_await ioChannelPtr_->performRead(&reader);
    co_return reader.readLen();
}

Task<> TcpConnection::write(const void * buf, size_t len)
{
    // 尝试获取写入权限
    bool expected = false;
    if (!writing_.compare_exchange_strong(expected, true, std::memory_order_acq_rel))
    {
        // 已有协程在写入，加入等待队列
        struct Awaiter
        {
            MpscQueue<std::coroutine_handle<>> * queue;

            bool await_ready() { return false; }
            void await_suspend(std::coroutine_handle<> h)
            {
                queue->push(h);
            }
            void await_resume() {}
        };

        co_await Awaiter{ &writeWaiters_ };
    }

    // 现在轮到我了，执行写入
    try
    {
        BufferWriter writer(buf, len);
        co_await ioChannelPtr_->performWrite(&writer);
    }
    catch (...)
    {
        // 释放锁并唤醒下一个
        if (auto next = writeWaiters_.pop())
        {
            Scheduler::current()->schedule(*next);
        }
        else
        {
            writing_.store(false, std::memory_order_release);
        }
        throw;
    }

    // 写入完成，唤醒下一个等待者
    if (auto next = writeWaiters_.pop())
    {
        Scheduler::current()->schedule(*next);
    }
    else
    {
        writing_.store(false, std::memory_order_release);
    }
}

} // namespace my_coro
