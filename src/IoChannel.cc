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
    scheduler_->registerIoChannel(this, [this](int fd, uint32_t ev) {
        handleIoEvents(fd, ev);
    });
}

IoChannel::~IoChannel()
{
    scheduler_->unregisterIoChannel(this);
}

void IoChannel::handleIoEvents(int fd, uint32_t ev)
{
    if (ev & (EPOLLERR | EPOLLHUP))
    {
        // channel->handleError();
        printf("Unhandled channel error for fd %d\n", fd_);
        return;
    }
    if (ev & EPOLLIN)
    {
        readable_ = true;
        if (readableWaiter_)
        {
            auto h = readableWaiter_;
            readableWaiter_ = nullptr;
            scheduler_->schedule(h);
        }
    }
    if (ev & EPOLLOUT)
    {
        printf("Handle write fd %d writable = %d\n", fd_, writable_);
        writable_ = true;
        if (writableWaiter_)
        {
            auto h = writableWaiter_;
            writableWaiter_ = nullptr;
            scheduler_->schedule(h);
        }
    }
}

bool IoChannel::ReadableAwaiter::await_ready() noexcept
{
    return channel_->readable_;
}

bool IoChannel::ReadableAwaiter::await_suspend(std::coroutine_handle<> h) noexcept
{
    if (channel_->readable_)
    {
        return false;
    }
    else
    {
        channel_->readableWaiter_ = h;
        return true;
    }
}

void IoChannel::ReadableAwaiter::await_resume() noexcept
{
}

bool IoChannel::WritableAwaiter::await_ready() noexcept
{
    return channel_->writable_;
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
