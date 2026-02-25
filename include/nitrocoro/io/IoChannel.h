/**
 * @file IoChannel.h
 * @brief IoChannel abstraction for managing fd I/O operations
 * @note Only supports one concurrent reader and one concurrent writer per fd
 *
 * THREAD SAFETY:
 * All member accesses are serialized by the single-threaded Scheduler event loop.
 * The execution order is: process_io_events() -> process_timers() -> resume_ready_coros().
 * handleReadable/handleWritable are called in process_io_events(), while coroutines
 * (performReadImpl/performWriteImpl and awaiters) execute in resume_ready_coros().
 * These phases never overlap, ensuring no race conditions on member variables.
 *
 * CRITICAL: Any future modifications MUST preserve this serialization guarantee.
 * Do NOT introduce concurrent access to IoChannel members from multiple threads.
 */
#pragma once

#include <coroutine>
#include <memory>
#include <nitrocoro/core/Scheduler.h>
#include <nitrocoro/core/Task.h>
#include <nitrocoro/core/Types.h>

namespace nitrocoro::io
{

using nitrocoro::Scheduler;
using nitrocoro::Task;

class IoChannel;
using IoChannelPtr = std::shared_ptr<IoChannel>;

class IoChannel
{
public:
    explicit IoChannel(int fd, TriggerMode mode = TriggerMode::EdgeTriggered, Scheduler * scheduler = Scheduler::current());
    ~IoChannel() noexcept;

    IoChannel(const IoChannel &) = delete;
    IoChannel & operator=(const IoChannel &) = delete;
    IoChannel(IoChannel &&) = delete;
    IoChannel & operator=(IoChannel &&) = delete;

    uint64_t id() const { return id_; }
    int fd() const { return fd_; }
    Scheduler * scheduler() const { return scheduler_; }
    TriggerMode triggerMode() const { return triggerMode_; }
    uint32_t events() const { return events_; }

    // Following methods MUST be called from Scheduler's thread
    void enableReading();
    void enableWriting();
    void disableReading();
    void disableWriting();
    void disableAll();

    enum class IoResult
    {
        Success,
        WouldBlock,
        Retry,
        Disconnect,
        Error
    };

    template <typename Reader>
        requires requires(Reader r, int fd, IoChannel * channel) {
            { r->read(fd, channel) } -> std::same_as<IoResult>;
        }
    Task<> performRead(Reader && reader)
    {
        co_return co_await performReadImpl(std::forward<Reader>(reader));
    }

    template <typename Func>
        requires std::invocable<Func, int, IoChannel *>
                 && std::same_as<std::invoke_result_t<Func, int, IoChannel *>, IoResult>
    Task<> performRead(Func && func)
    {
        co_return co_await performReadImpl(std::forward<Func>(func));
    }

    template <typename Writer>
        requires requires(Writer w, int fd, IoChannel * channel) {
            { w->write(fd, channel) } -> std::same_as<IoResult>;
        }
    Task<> performWrite(Writer && writer)
    {
        co_return co_await performWriteImpl(std::forward<Writer>(writer));
    }

    template <typename Func>
        requires std::invocable<Func, int, IoChannel *>
                 && std::same_as<std::invoke_result_t<Func, int, IoChannel *>, IoResult>
    Task<> performWrite(Func && func)
    {
        co_return co_await performWriteImpl(std::forward<Func>(func));
    }

    void cancelRead();
    void cancelWrite();
    void cancelAll();

private:
    struct IoState
    {
        int fd{ -1 };
        bool readable{ false };
        bool writable{ true };
        std::coroutine_handle<> readableWaiter;
        std::coroutine_handle<> writableWaiter;
        bool readCanceled{ false };
        bool writeCanceled{ false };
    };

    // Called by Scheduler::process_io_events() when epoll reports events
    static void handleIoEvents(Scheduler * scheduler, IoState * state, uint32_t ev);

    struct [[nodiscard]] ReadableAwaiter
    {
        IoState * state_;

        bool await_ready() noexcept;
        bool await_suspend(std::coroutine_handle<> h) noexcept;
        void await_resume();
    };

    struct [[nodiscard]] WritableAwaiter
    {
        IoState * state_;

        bool await_ready() noexcept;
        bool await_suspend(std::coroutine_handle<> h) noexcept;
        void await_resume();
    };

    template <typename T>
    Task<> performReadImpl(T && funcOrReader)
    {
        co_await scheduler_->switch_to();
        while (true)
        {
            if (!state_->readable)
            {
                co_await ReadableAwaiter{ state_.get() };
            }

            IoResult result;
            if constexpr (std::is_pointer_v<std::remove_reference_t<T>>)
                result = funcOrReader->read(fd_, this);
            else
                result = funcOrReader(fd_, this);

            switch (result)
            {
                case IoResult::Success:
                    if (triggerMode_ == TriggerMode::LevelTriggered)
                    {
                        state_->readable = false;
                    }
                    co_return;

                case IoResult::WouldBlock:
                    state_->readable = false;
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
        co_await scheduler_->switch_to();
        while (true)
        {
            if (!state_->writable)
            {
                co_await WritableAwaiter{ state_.get() };
            }

            IoResult result;
            if constexpr (std::is_pointer_v<std::remove_reference_t<T>>)
                result = funcOrWriter->write(fd_, this);
            else
                result = funcOrWriter(fd_, this);

            switch (result)
            {
                case IoResult::Success:
                    co_return;

                case IoResult::WouldBlock:
                    state_->writable = false;
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

    const uint64_t id_;
    int fd_{ -1 };
    Scheduler * scheduler_{ nullptr };
    TriggerMode triggerMode_{ TriggerMode::EdgeTriggered };

    // All members below are accessed only within Scheduler's single thread.
    // No synchronization primitives needed due to serialized execution model.
    uint32_t events_{ 0 };
    std::shared_ptr<IoState> state_;
};

} // namespace nitrocoro::io
