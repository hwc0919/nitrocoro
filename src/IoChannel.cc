/**
 * @file IoChannel.cc
 * @brief Implementation of IoChannel
 */
#include <nitrocoro/core/Scheduler.h>
#include <nitrocoro/io/IoChannel.h>
#include <nitrocoro/utils/Debug.h>

#include <arpa/inet.h>
#include <cassert>
#include <cstring>
#include <sys/epoll.h>

namespace nitrocoro::io
{

IoChannel::IoChannel(int fd, TriggerMode mode, Scheduler * scheduler)
    : id_(Scheduler::nextIoId())
    , fd_(fd)
    , scheduler_(scheduler)
    , triggerMode_(mode)
    , state_(std::make_shared<IoState>(fd))
{
    assert(scheduler_ != nullptr);

    scheduler->schedule([id = id_, fd, weakState = std::weak_ptr(state_), scheduler]() {
        if (auto _ = weakState.lock())
        {
            scheduler->setIoHandler(id, fd, [scheduler, weakState](int fd, uint32_t ev) {
                if (auto state = weakState.lock())
                {
                    assert(fd == state->fd);
                    handleIoEvents(scheduler, state.get(), ev);
                }
            });
        }
    });
}

IoChannel::~IoChannel() noexcept
{
    scheduler_->schedule([id = id_, scheduler = scheduler_, guard = std::move(guard_)]() mutable {
        scheduler->removeIo(id);
        guard.reset();
    });
}

void IoChannel::handleIoEvents(Scheduler * scheduler, IoState * state, uint32_t ev)
{
    if ((ev & EPOLLHUP) && !(ev & EPOLLIN))
    {
        // peer closed, and no more bytes to read
        NITRO_TRACE("Peer closed, fd %d\n", state->fd);
        // TODO: handle close
    }

    if (ev & EPOLLERR) // (POLLNVAL | POLLERR)
    {
        NITRO_ERROR("Channel error for fd %d\n", state->fd);
        int error = 0;
        socklen_t len = sizeof(error);
        if (getsockopt(state->fd, SOL_SOCKET, SO_ERROR, &error, &len) < 0)
        {
            NITRO_ERROR("getsockopt failed: %s\n", strerror(errno));
        }
        if (error == 0)
        {
            NITRO_DEBUG("EPOLLERR but no error\n");
        }
        else
        {
            NITRO_ERROR("socket %d error %d: %s\n", state->fd, error, strerror(error));
        }
        // TODO: mark error
    }

    if (ev & EPOLLIN) // (POLLIN | POLLPRI | POLLRDHUP)
    {
        state->readable = true;
        if (state->readableWaiter)
        {
            auto h = state->readableWaiter;
            state->readableWaiter = nullptr;
            scheduler->schedule(h);
        }
    }
    if (ev & EPOLLOUT) // WIN32: if ((ev & POLLOUT) && !(ev & POLLHUP))
    {
        NITRO_DEBUG("Handle write fd %d writable = %d\n", state->fd, state->writable);
        state->writable = true;
        if (state->writableWaiter)
        {
            auto h = state->writableWaiter;
            state->writableWaiter = nullptr;
            scheduler->schedule(h);
        }
    }
}

bool IoChannel::ReadableAwaiter::await_ready() noexcept
{
    return state_->readable;
}

bool IoChannel::ReadableAwaiter::await_suspend(std::coroutine_handle<> h) noexcept
{
    if (state_->readable)
    {
        return false;
    }
    else
    {
        state_->readableWaiter = h;
        return true;
    }
}

void IoChannel::ReadableAwaiter::await_resume() noexcept
{
}

bool IoChannel::WritableAwaiter::await_ready() noexcept
{
    return state_->writable;
}

bool IoChannel::WritableAwaiter::await_suspend(std::coroutine_handle<> h) noexcept
{
    if (state_->writable)
    {
        return false;
    }
    else
    {
        state_->writableWaiter = h;
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
        scheduler_->updateIo(id_, fd_, events_, triggerMode_);
    }
}

void IoChannel::disableReading()
{
    if (events_ & EPOLLIN)
    {
        events_ &= ~EPOLLIN;
        scheduler_->updateIo(id_, fd_, events_, triggerMode_);
    }
}

void IoChannel::enableWriting()
{
    if (!(events_ & EPOLLOUT))
    {
        events_ |= EPOLLOUT;
        scheduler_->updateIo(id_, fd_, events_, triggerMode_);
    }
}

void IoChannel::disableWriting()
{
    if (events_ & EPOLLOUT)
    {
        events_ &= ~EPOLLOUT;
        scheduler_->updateIo(id_, fd_, events_, triggerMode_);
    }
}

void IoChannel::disableAll()
{
    if (events_ != 0)
    {
        events_ = 0;
        scheduler_->updateIo(id_, fd_, events_, triggerMode_);
    }
}

void IoChannel::cancelRead()
{
    if (state_->readableWaiter)
    {
        state_->readCanceled = true;
        auto h = state_->readableWaiter;
        state_->readableWaiter = nullptr;
        scheduler_->schedule(h);
    }
}

void IoChannel::cancelWrite()
{
    if (state_->writableWaiter)
    {
        state_->writeCanceled = true;
        auto h = state_->writableWaiter;
        state_->writableWaiter = nullptr;
        scheduler_->schedule(h);
    }
}

void IoChannel::cancelAll()
{
    cancelRead();
    cancelWrite();
}

} // namespace nitrocoro::io
