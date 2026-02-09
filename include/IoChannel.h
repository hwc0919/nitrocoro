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

enum class TriggerMode
{
    EdgeTriggered,
    LevelTriggered
};

class IoChannel
{
public:
    IoChannel(int fd, CoroScheduler * scheduler, TriggerMode mode = TriggerMode::EdgeTriggered);
    ~IoChannel();

    IoChannel(const IoChannel &) = delete;
    IoChannel & operator=(const IoChannel &) = delete;
    IoChannel(IoChannel &&) = delete;
    IoChannel & operator=(IoChannel &&) = delete;

    struct [[nodiscard]] ReadableAwaiter
    {
        IoChannel * channel_;

        bool await_ready() noexcept
        {
            return channel_->readable_;
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
        void await_resume() noexcept {}
    };

    struct [[nodiscard]] WriteAwaiter
    {
        bool await_ready() noexcept
        {
            if (!channel_->writable_)
            {
                return false; // suspend
            }
            if (len_ == 0)
            {
                return true;
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

        bool await_suspend(std::coroutine_handle<> h) noexcept;
        ssize_t await_resume() noexcept(false)
        {
            if (len_ == 0)
            {
                return 0;
            }
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

    ReadableAwaiter readable();
    Task<int> accept();
    Task<ssize_t> read(void * buf, size_t len);
    Task<ssize_t> write(const void * buf, size_t len);

private:
    friend class CoroScheduler;

    void handleReadable();
    void handleWritable();

    int fd_{ -1 };
    CoroScheduler * scheduler_{ nullptr };
    TriggerMode triggerMode_{ TriggerMode::EdgeTriggered };

    uint32_t events_{ 0 };
    bool readable_{ false };
    bool writable_{ true };

    std::coroutine_handle<> pendingRead_;
    std::coroutine_handle<> pendingWrite_;
};

} // namespace my_coro
