/**
 * @file IoChannel.cc
 * @brief Implementation of IoChannel
 */
#include "IoChannel.h"
#include "Scheduler.h"
#include <arpa/inet.h>
#include <cstring>
#include <sys/epoll.h>

namespace my_coro
{

IoChannelPtr IoChannel::create(int fd, Scheduler * scheduler, TriggerMode mode)
{
    auto channel = std::shared_ptr<IoChannel>(new IoChannel(fd, scheduler, mode));
    scheduler->schedule([weakChannel = std::weak_ptr(channel), scheduler = scheduler]() {
        if (auto thisPtr = weakChannel.lock())
        {
            scheduler->setIoChannelHandler(thisPtr, [weakChannel](int fd, uint32_t ev) {
                if (auto thisPtr = weakChannel.lock())
                {
                    thisPtr->handleIoEvents(ev);
                }
            });
        }
    });
    return channel;
}

IoChannel::IoChannel(int fd, Scheduler * scheduler, TriggerMode mode)
    : fd_(fd), scheduler_(scheduler), triggerMode_(mode)
{
}

IoChannel::~IoChannel()
{
    scheduler_->schedule([id = id_, scheduler = scheduler_]() {
        scheduler->removeIoChannel(id);
    });
}

void IoChannel::handleIoEvents(uint32_t ev)
{
    if ((ev & EPOLLHUP) && !(ev & EPOLLIN))
    {
        // peer closed, and no more bytes to read
        printf("Peer closed, fd %d\n", fd_);
        // TODO: handle close
    }

    if (ev & EPOLLERR) // (POLLNVAL | POLLERR)
    {
        printf("Channel error for fd %d\n", fd_);
        int error = 0;
        socklen_t len = sizeof(error);
        if (getsockopt(fd_, SOL_SOCKET, SO_ERROR, &error, &len) < 0)
        {
            printf("getsockopt failed: %s\n", strerror(errno));
        }
        if (error == 0)
        {
            printf("EPOLLERR but no error\n");
        }
        else
        {
            printf("socket %d error %d: %s\n", fd_, error, strerror(error));
        }
        // TODO: mark error
    }

    if (ev & EPOLLIN) // (POLLIN | POLLPRI | POLLRDHUP)
    {
        readable_ = true;
        if (readableWaiter_)
        {
            auto h = readableWaiter_;
            readableWaiter_ = nullptr;
            scheduler_->schedule(h);
        }
    }
    if (ev & EPOLLOUT) // WIN32: if ((ev & POLLOUT) && !(ev & POLLHUP))
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
        return true;
    }
}

void IoChannel::WritableAwaiter::await_resume() noexcept
{
}

void IoChannel::enableReading()
{
    if (!(events_ & EPOLLIN))
    {
        events_ |= EPOLLIN;
        scheduler_->updateIoChannel(shared_from_this());
    }
}

void IoChannel::disableReading()
{
    if (events_ & EPOLLIN)
    {
        events_ &= ~EPOLLIN;
        scheduler_->updateIoChannel(shared_from_this());
    }
}

void IoChannel::enableWriting()
{
    if (!(events_ & EPOLLOUT))
    {
        events_ |= EPOLLOUT;
        scheduler_->updateIoChannel(shared_from_this());
    }
}

void IoChannel::disableWriting()
{
    if (events_ & EPOLLOUT)
    {
        events_ &= ~EPOLLOUT;
        scheduler_->updateIoChannel(shared_from_this());
    }
}

} // namespace my_coro
