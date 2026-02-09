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

Task<> IoChannel::write(const void * buf, size_t len)
{
    printf("Write %p %zu\n", buf, len);
    WriteOp op{ buf, len };

    struct Awaiter
    {
        WriteOp * op;
        IoChannel * ch;

        bool await_ready() { return false; }
        void await_suspend(std::coroutine_handle<> h)
        {
            op->handle_ = h;
            ch->pendingWrites_.push(op);
            ch->wakeupWriteTask();
        }
        void await_resume()
        {
            if (op->error_ != 0)
            {
                if (op->error_ == ECONNREFUSED)
                {
                    throw std::runtime_error("Connection refused");
                }
                throw std::runtime_error("write error " + std::to_string(op->error_));
            }
        }
    };

    co_await Awaiter{ &op, this };
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

IoChannel::ReadableAwaiter IoChannel::readable()
{
    return ReadableAwaiter{ this };
}

IoChannel::WritableAwaiter IoChannel::writable()
{
    return WritableAwaiter{ this };
}

void IoChannel::wakeupWriteTask()
{
    if (writeTaskSuspended_) // 必须检查！
    {
        writeTaskSuspended_ = false; // 先清除标志
        scheduler_->schedule(writeTask_.handle_);
    }
}

Task<void> IoChannel::writeTaskLoop()
{
    while (writeTaskRunning_) // TODO: stop when close write
    {
        while (pendingWrites_.empty() && writeTaskRunning_)
        {
            writeTaskSuspended_ = true;
            co_await std::suspend_always{};
        }
        if (!writeTaskRunning_)
            break;

        WriteOp * op = pendingWrites_.front();
        pendingWrites_.pop();

        size_t totalWrite{ 0 };
        do
        {
            if (!writable_)
            {
                co_await WritableAwaiter{ this };
            }

            const char * ptr = static_cast<const char *>(op->buf_) + totalWrite;
            size_t remaining = op->len_ - totalWrite;
            ssize_t n = ::write(fd_, ptr, remaining);
            int lastErrno = errno;
            if (n > 0)
            {
                totalWrite += n;
            }
            else if (lastErrno == 0 || lastErrno == EINTR)
            {
                continue;
            }
            else if (
                lastErrno == EINPROGRESS
                || lastErrno == EALREADY || lastErrno == EAGAIN
#if EWOULDBLOCK != EAGAIN
                || lastErrno == EWOULDBLOCK
#endif
            )
            {
                writable_ = false;
                continue;
            }
            else
            {
                op->error_ = lastErrno;
                break;
            }
        } while (totalWrite < op->len_);

        op->written_ = totalWrite;
        scheduler_->schedule(op->handle_);
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
