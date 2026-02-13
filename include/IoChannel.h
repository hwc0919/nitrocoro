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

class Scheduler;

enum class TriggerMode
{
    EdgeTriggered,
    LevelTriggered
};

class IoChannel
{
public:
    IoChannel(int fd, Scheduler * scheduler, TriggerMode mode = TriggerMode::EdgeTriggered);
    ~IoChannel();

    IoChannel(const IoChannel &) = delete;
    IoChannel & operator=(const IoChannel &) = delete;
    IoChannel(IoChannel &&) = delete;
    IoChannel & operator=(IoChannel &&) = delete;

    int fd() const { return fd_; }
    Scheduler * scheduler() const { return scheduler_; }
    TriggerMode triggerMode() const { return triggerMode_; }

    struct [[nodiscard]] ReadableAwaiter
    {
        IoChannel * channel_;

        bool await_ready() noexcept;
        bool await_suspend(std::coroutine_handle<> h) noexcept;
        void await_resume() noexcept;
    };

    struct [[nodiscard]] WritableAwaiter
    {
        IoChannel * channel_;

        bool await_ready() noexcept;
        bool await_suspend(std::coroutine_handle<> h) noexcept;
        void await_resume() noexcept;
    };

    enum class IoResult
    {
        Success,    // 操作成功
        WouldBlock, // EAGAIN，需要等待
        Retry,      // EINTR，立即重试
        Disconnect, // 连接中断
        Error       // 错误
    };

    template <typename Reader>
        requires requires(Reader r, int fd) {
            { r->read(fd) } -> std::same_as<IoResult>;
        }
    Task<> performRead(Reader && reader)
    {
        co_return co_await performReadImpl(std::forward<Reader>(reader));
    }

    template <typename Func>
        requires std::invocable<Func, int> && std::same_as<std::invoke_result_t<Func, int>, IoResult>
    Task<> performRead(Func && func)
    {
        co_return co_await performReadImpl(std::forward<Func>(func));
    }

    template <typename Writer>
        requires requires(Writer w, int fd) {
            { w->write(fd) } -> std::same_as<IoResult>;
        }
    Task<> performWrite(Writer && writer)
    {
        co_return co_await performWriteImpl(std::forward<Writer>(writer));
    }

    template <typename Func>
        requires std::invocable<Func, int> && std::same_as<std::invoke_result_t<Func, int>, IoResult>
    Task<> performWrite(Func && func)
    {
        co_return co_await performWriteImpl(std::forward<Func>(func));
    }

private:
    friend class Scheduler;

    template <typename T>
    Task<> performReadImpl(T && funcOrReader)
    {
        while (true)
        {
            if (!readable_)
            {
                co_await ReadableAwaiter{ this };
            }

            IoResult result;
            if constexpr (std::is_pointer_v<std::remove_reference_t<T>>)
                result = funcOrReader->read(fd_);
            else
                result = funcOrReader(fd_);

            switch (result)
            {
                case IoResult::Success:
                    if (triggerMode_ == TriggerMode::LevelTriggered)
                    {
                        readable_ = false;
                    }
                    co_return;

                case IoResult::WouldBlock:
                    readable_ = false;
                    break;

                case IoResult::Retry:
                    break;

                case IoResult::Disconnect:
                    throw std::runtime_error("I/O disconnect");

                case IoResult::Error:
                default:
                    throw std::runtime_error("I/O read error");
            }
        }
    }

    template <typename T>
    Task<> performWriteImpl(T && funcOrWriter)
    {
        while (true)
        {
            if (!writable_)
            {
                co_await WritableAwaiter{ this };
            }

            IoResult result;
            if constexpr (std::is_pointer_v<std::remove_reference_t<T>>)
                result = funcOrWriter->write(fd_);
            else
                result = funcOrWriter(fd_);

            switch (result)
            {
                case IoResult::Success:
                    co_return;

                case IoResult::WouldBlock:
                    writable_ = false;
                    break;

                case IoResult::Retry:
                    break;

                case IoResult::Disconnect:
                    throw std::runtime_error("I/O disconnect");

                case IoResult::Error:
                default:
                    throw std::runtime_error("I/O write error");
            }
        }
    }

    void handleReadable();
    void handleWritable();

    int fd_{ -1 };
    Scheduler * scheduler_{ nullptr };
    TriggerMode triggerMode_{ TriggerMode::EdgeTriggered };

    uint32_t events_{ 0 };
    bool readable_{ false };
    bool writable_{ true };

    std::coroutine_handle<> readableWaiter_;
    std::coroutine_handle<> writableWaiter_;
};

struct BufferReader
{
    BufferReader(void * buf, size_t len) : buf_(buf), len_(len) {}
    ssize_t readLen() const { return readLen_; }

    IoChannel::IoResult read(int fd)
    {
        if (!buf_ || len_ == 0)
        {
            return IoChannel::IoResult::Success;
        }

        ssize_t ret = ::read(fd, buf_, len_);
        if (ret > 0)
        {
            readLen_ = ret;
            return IoChannel::IoResult::Success;
        }
        else if (ret == 0)
        {
            return IoChannel::IoResult::Disconnect;
        }
        else
        {
            switch (errno)
            {
                case EAGAIN:
#if EAGAIN != EWOULDBLOCK
                case EWOULDBLOCK:
#endif
                    return IoChannel::IoResult::WouldBlock;
                case EINTR:
                    return IoChannel::IoResult::Retry;
                default:
                    return IoChannel::IoResult::Error;
            }
        }
    }

private:
    void * buf_;
    size_t len_;
    ssize_t readLen_{ 0 };
};

struct BufferWriter
{
    BufferWriter(const void * buf, size_t len) : buf_(buf), len_(len)
    {
    }

    IoChannel::IoResult write(int fd)
    {
        if (!buf_ || len_ == 0)
        {
            return IoChannel::IoResult::Success;
        }

        ssize_t ret = ::write(fd, static_cast<const char *>(buf_) + wroteLen_, len_ - wroteLen_);
        if (ret > 0)
        {
            wroteLen_ += ret;
            if (wroteLen_ >= static_cast<ssize_t>(len_))
            {
                return IoChannel::IoResult::Success;
            }
            else
            {
                return IoChannel::IoResult::Retry;
            }
        }
        else // ret <= 0
        {
            switch (errno)
            {
                case EAGAIN:
#if EAGAIN != EWOULDBLOCK
                case EWOULDBLOCK:
#endif
                    return IoChannel::IoResult::WouldBlock;
                case EINTR:
                    return IoChannel::IoResult::Retry;
                case EPIPE:
                case ECONNRESET:
                    return IoChannel::IoResult::Disconnect;
                default:
                    return IoChannel::IoResult::Error;
            }
        }
    }

private:
    const void * buf_;
    size_t len_;
    ssize_t wroteLen_{ 0 };
};

} // namespace my_coro
