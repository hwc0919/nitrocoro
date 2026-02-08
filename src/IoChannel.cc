/**
 * @file IoChannel.cc
 * @brief Implementation of IoChannel
 */
#include "IoChannel.h"
#include "CoroScheduler.h"
#include <sys/epoll.h>

namespace my_coro
{

IoChannel::IoChannel(int fd, CoroScheduler * scheduler, TriggerMode mode)
    : fd_(fd), scheduler_(scheduler), triggerMode_(mode), events_(EPOLLIN)
{
    scheduler_->registerIoChannel(this);
}

IoChannel::~IoChannel()
{
    // TODO: clear all pending handles
    scheduler_->unregisterIoChannel(this);
}

Task<ssize_t> IoChannel::read(void * buf, size_t len)
{
    ssize_t result = co_await ReadAwaiter{
        .channel_ = this,
        .buf_ = buf,
        .len_ = len
    };

    co_return result;
}

Task<ssize_t> IoChannel::write(const void * buf, size_t len)
{
    ssize_t result = co_await WriteAwaiter{
        .channel_ = this,
        .buf_ = buf,
        .len_ = len
    };

    co_return result;
}

void IoChannel::handleReadable()
{
    //    assert(readable_ == false);
    readable_ = true;
    if (pendingRead_)
    {
        auto h = pendingRead_;
        pendingRead_ = nullptr;
        if (h)
        {
            scheduler_->schedule(h);
        }
    }
}

void IoChannel::handleWritable()
{
    printf("Handle write fd %d writable = %d\n", fd_, writable_);
    writable_ = true;
    if (pendingWrite_)
    {
        auto h = pendingWrite_;
        pendingWrite_ = nullptr;
        events_ &= ~EPOLLOUT;
        scheduler_->updateChannel(this);
        if (h)
        {
            scheduler_->schedule(h);
        }
    }
}

bool IoChannel::WriteAwaiter::await_suspend(std::coroutine_handle<> h) noexcept
{
    if (channel_->writable_)
    {
        return false;
    }
    else
    {
        channel_->pendingWrite_ = h;
        channel_->events_ |= EPOLLOUT;
        channel_->scheduler_->updateChannel(channel_);
        return true;
    }
}

} // namespace my_coro
