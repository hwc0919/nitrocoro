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
                channel_->readableWaiter_ = h;
                return true;
            }
        }
        void await_resume() noexcept {}
    };

    struct [[nodiscard]] WritableAwaiter
    {
        IoChannel * channel_;

        bool await_ready() noexcept
        {
            return channel_->writable_;
        }
        bool await_suspend(std::coroutine_handle<> h) noexcept;
        void await_resume() noexcept;
    };

    ReadableAwaiter readable();
    WritableAwaiter writable();
    Task<int> accept();
    Task<ssize_t> read(void * buf, size_t len);
    Task<> write(const void * buf, size_t len);

    enum class IoResult
    {
        Success,    // 操作成功
        WouldBlock, // EAGAIN，需要等待
        Retry,      // EINTR，立即重试
        Error       // 错误
    };

    struct IoReader
    {
        virtual ~IoReader() = default;
        virtual IoResult read(int fd) = 0;
    };

    struct IoWriter
    {
        virtual ~IoWriter() = default;
        virtual IoResult write(int fd) = 0;
    };

    Task<> performRead(IoReader * reader)
    {
        co_return co_await performRead([reader](int fd) { return reader->read(fd); });
    }

    Task<> performWrite(IoWriter * writer)
    {
        co_return co_await performWrite([writer](int fd) { return writer->write(fd); });
    }

    template <typename Func>
        requires std::invocable<Func, int> && std::same_as<std::invoke_result_t<Func, int>, IoResult>
    Task<> performRead(Func && func)
    {
        while (true)
        {
            if (!readable_)
            {
                co_await ReadableAwaiter{ this };
            }

            IoResult result = func(fd_);

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

                case IoResult::Error:
                    throw std::runtime_error("I/O read error");
            }
        }
    }

    template <typename Func>
        requires std::invocable<Func, int> && std::same_as<std::invoke_result_t<Func, int>, IoResult>
    Task<> performWrite(Func && func)
    {
        while (true)
        {
            if (!writable_)
            {
                co_await WritableAwaiter{ this };
            }

            IoResult result = func(fd_);

            switch (result)
            {
                case IoResult::Success:
                    co_return;

                case IoResult::WouldBlock:
                    writable_ = false;
                    break;

                case IoResult::Retry:
                    break;

                case IoResult::Error:
                    throw std::runtime_error("I/O write error");
            }
        }
    }

private:
    friend class Scheduler;

    void handleReadable();
    void handleWritable();

    Task<void> writeTaskLoop();

    int fd_{ -1 };
    Scheduler * scheduler_{ nullptr };
    TriggerMode triggerMode_{ TriggerMode::EdgeTriggered };

    uint32_t events_{ 0 };
    bool readable_{ false };
    bool writable_{ true };

    std::coroutine_handle<> readableWaiter_;
    std::coroutine_handle<> writableWaiter_;

    struct WriteOp
    {
        const void * buf_;
        size_t len_;
        std::coroutine_handle<> handle_;
        size_t written_{ 0 };
        int error_{ 0 };
    };
    bool writeTaskRunning_{ true };
    Task<> writeTask_{ writeTaskLoop() };
    bool writeTaskSuspended_{ true };
    std::queue<WriteOp *> pendingWrites_;

    void wakeupWriteTask();
};

} // namespace my_coro
