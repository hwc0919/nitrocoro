/**
 * @file IoChannel.cc
 * @brief Implementation of IoChannel
 */
#include "IoChannel.h"
#include "CoroScheduler.h"
#include <arpa/inet.h>
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

Task<int> IoChannel::accept()
{
    while (true)
    {
        if (!readable_)
        {
            co_await ReadableAwaiter{ this };
        }

        sockaddr_in client_addr{};
        socklen_t addr_len = sizeof(client_addr);
        int client_fd = ::accept4(fd_,
                                  (struct sockaddr *)&client_addr,
                                  &addr_len,
                                  SOCK_NONBLOCK | SOCK_CLOEXEC);
        int lastErrno = errno;
        if (client_fd >= 0)
        {
            if (triggerMode_ == TriggerMode::LevelTriggered)
            {
                readable_ = false;
            }
            co_return client_fd;
        }
        else // < 0
        {
            switch (lastErrno)
            {
#if EAGAIN != EWOULDBLOCK
                case EAGAIN:
#endif
                case EWOULDBLOCK:
                    readable_ = false;
                    break;
                case EINTR: // interrupted by signal
                    break;
                default:
                    // 其他错误，关闭连接
                    ::close(fd_);
                    // closed_ = true;
                    throw std::runtime_error("read error " + std::to_string(lastErrno) + " " + std::to_string(client_fd));
            }
        }
    }
}

Task<ssize_t> IoChannel::read(void * buf, size_t len)
{
    if (len == 0)
    {
        co_return 0;
    }
    while (true)
    {
        if (!readable_)
        {
            co_await ReadableAwaiter{ this };
        }

        ssize_t result = ::read(fd_, buf, len);
        int lastErrno = errno;
        if (result > 0)
        {
            if (triggerMode_ == TriggerMode::LevelTriggered)
            {
                readable_ = false;
            }
            co_return result;
        }
        else if (result == 0)
        {
            ::close(fd_);
            // closed_ = true;
            throw std::runtime_error("Read error: peer closed");
        }
        else // < 0
        {
            switch (lastErrno)
            {
#if EAGAIN != EWOULDBLOCK
                case EAGAIN:
#endif
                case EWOULDBLOCK:
                    readable_ = false;
                    break;

                case EINTR: // interrupted by signal
                    break;

                case ECONNRESET: // reset by peer
                case EPIPE:      // bad connection
                default:
                    // 其他错误，关闭连接
                    ::close(fd_);
                    // closed_ = true;
                    throw std::runtime_error("read error " + std::to_string(lastErrno) + " " + std::to_string(result));
            }
        }
    }
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
IoChannel::ReadableAwaiter IoChannel::readable()
{
    return ReadableAwaiter{ this };
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
