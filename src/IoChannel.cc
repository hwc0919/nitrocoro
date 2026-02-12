/**
 * @file IoChannel.cc
 * @brief Implementation of IoChannel
 */
#include "IoChannel.h"
#include "Scheduler.h"
#include <arpa/inet.h>
#include <sys/epoll.h>

namespace my_coro
{

IoChannel::IoChannel(int fd, Scheduler * scheduler, TriggerMode mode)
    : fd_(fd), scheduler_(scheduler), triggerMode_(mode), events_(EPOLLIN)
{
    scheduler_->registerIoChannel(this);
}

IoChannel::~IoChannel()
{
    // TODO: clear all pending handles
    scheduler_->unregisterIoChannel(this);
}

void IoChannel::handleReadable()
{
    //    assert(readable_ == false);
    readable_ = true;
    if (readableWaiter_)
    {
        auto h = readableWaiter_;
        readableWaiter_ = nullptr;
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
    if (writableWaiter_)
    {
        auto h = writableWaiter_;
        writableWaiter_ = nullptr;
        if (h)
        {
            scheduler_->schedule(h);
        }
    }
}

bool IoChannel::WritableAwaiter::await_suspend(std::coroutine_handle<> h) noexcept
{
    if (channel_->writable_)
    {
        return false;
    }
    else
    {
        channel_->writableWaiter_ = h;
        channel_->events_ |= EPOLLOUT;
        channel_->scheduler_->updateChannel(channel_);
        return true;
    }
}

void IoChannel::WritableAwaiter::await_resume() noexcept
{
    channel_->events_ &= ~EPOLLOUT;
    channel_->scheduler_->updateChannel(channel_);
}

} // namespace my_coro
