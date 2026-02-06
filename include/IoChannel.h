/**
 * @file IoChannel.h
 * @brief IoChannel abstraction for managing fd I/O operations with multiple concurrent readers/writers
 */
#pragma once

#include "Task.h"
#include <coroutine>
#include <memory>
#include <queue>

namespace my_coro
{

class CoroScheduler;

class IoChannel
{
public:
    IoChannel(int fd, CoroScheduler * scheduler);
    ~IoChannel();

    IoChannel(const IoChannel &) = delete;
    IoChannel & operator=(const IoChannel &) = delete;
    IoChannel(IoChannel &&) = delete;
    IoChannel & operator=(IoChannel &&) = delete;

    struct [[nodiscard]] ReadAwaiter
    {
        bool await_ready() noexcept
        {
            if (!channel_->readable_)
            {
                return false; // suspend
            }
            // try read first
            result_ = ::read(channel_->fd_, buf_, len_);
            lastErrno_ = errno;
            hasRead_ = true;
            if (result_ < 0)
            {
                if (lastErrno_ == EAGAIN
#if EWOULDBLOCK != EAGAIN
                    || lastErrno_ == EWOULDBLOCK
#endif
                )
                {
                    channel_->readable_ = false;
                    return false; // suspend
                }
                else if (lastErrno_ == EINTR)
                {
                    return true;
                }
            }
            return true;
        }
        bool await_suspend(std::coroutine_handle<> h) noexcept
        {
            if (channel_->readable_)
            {
                return false;
            }
            else
            {
                channel_->pendingRead_ = h;
                return true;
            }
        }
        ssize_t await_resume() noexcept(false)
        {
            if (!hasRead_)
            {
                result_ = ::read(channel_->fd_, buf_, len_);
                lastErrno_ = errno;
            }
            if (result_ > 0)
            {
                return result_;
            }
            else if (result_ == 0)
            {
                // peer closed
                // TODO: close(fd)
                // TODO: EINTR?
                throw std::runtime_error("peer closed");
            }
            else
            {
                switch (lastErrno_)
                {
#if EAGAIN != EWOULDBLOCK
                    case EAGAIN:
#endif
                    case EWOULDBLOCK: // should not happen again!
                        channel_->readable_ = false;
                        return 0;

                    case EINTR: // interrupted by signal
                        return 0;

                    case ECONNRESET: // reset by peer
                    case EPIPE:      // bad connection
                    default:
                        // 其他错误，关闭连接
                        // close(fd)
                        throw std::runtime_error("read error " + std::to_string(lastErrno_) + " " + std::to_string(result_));
                }
            }
        }

        IoChannel * channel_;
        void * buf_;
        size_t len_;

        bool hasRead_{ false };
        ssize_t result_{ -1 };
        int lastErrno_{ 0 };
    };

    struct [[nodiscard]] WriteAwaiter
    {
        bool await_ready() noexcept
        {
            if (!channel_->writable_)
            {
                return false; // suspend
            }
            // try to write first
            result_ = ::write(channel_->fd_, buf_, len_);
            lastErrno_ = errno;
            hasWritten_ = true;
            if (result_ < 0)
            {
                if (lastErrno_ == EAGAIN
#if EWOULDBLOCK != EAGAIN
                    || lastErrno_ == EWOULDBLOCK
#endif
                )
                {
                    channel_->writable_ = false;
                    return false; // suspend
                }
                else if (lastErrno_ == EINTR)
                {
                    return true;
                }
            }
            return true;
        }

        bool await_suspend(std::coroutine_handle<> h) noexcept
        {
            if (channel_->writable_)
            {
                return false;
            }
            else
            {
                channel_->pendingWrite_ = h;
                return true;
            }
        }

        ssize_t await_resume() noexcept(false)
        {
            if (!hasWritten_)
            {
                result_ = ::write(channel_->fd_, buf_, len_);
                lastErrno_ = errno;
            }
            if (result_ > 0)
            {
                return result_;
            }
            else if (result_ == 0)
            {
                // write 返回 0 通常不应该发生
                // TODO: EINTR?
                throw std::runtime_error("write returned 0");
            }
            else
            {
                switch (lastErrno_)
                {
#if EAGAIN != EWOULDBLOCK
                    case EAGAIN:
#endif
                    case EWOULDBLOCK: // should not happen again!
                        channel_->writable_ = false;
                        return 0;

                    case EINTR: // interrupted by signal
                        return 0;

                    case ECONNRESET: // reset by peer
                    case EPIPE:      // broken pipe
                    case ECONNABORTED:
                    default:
                        throw std::runtime_error("write error");
                }
            }
        }

        IoChannel * channel_;
        const void * buf_;
        size_t len_;

        bool hasWritten_{ false };
        ssize_t result_{ -1 };
        int lastErrno_{ 0 };
    };

    Task<ssize_t> read(void * buf, size_t len);
    Task<ssize_t> write(const void * buf, size_t len);

private:
    friend class CoroScheduler;

    void handleReadable();
    void handleWritable();

    int fd_{ -1 };
    CoroScheduler * scheduler_{ nullptr };
    bool readable_{ false };
    bool writable_{ false };

    std::coroutine_handle<> pendingRead_;
    std::coroutine_handle<> pendingWrite_;
};

} // namespace my_coro
