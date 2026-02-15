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

#include "Scheduler.h"
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

class IoChannel;
using IoChannelPtr = std::shared_ptr<IoChannel>;

class IoChannel : public std::enable_shared_from_this<IoChannel>
{
public:
    static IoChannelPtr create(int fd, Scheduler * scheduler, TriggerMode mode = TriggerMode::EdgeTriggered);
    ~IoChannel();

    IoChannel(const IoChannel &) = delete;
    IoChannel & operator=(const IoChannel &) = delete;
    IoChannel(IoChannel &&) = delete;
    IoChannel & operator=(IoChannel &&) = delete;

    uint64_t id() const { return id_; }
    int fd() const { return fd_; }
    Scheduler * scheduler() const { return scheduler_; }
    TriggerMode triggerMode() const { return triggerMode_; }
    uint32_t events() const { return events_; }

    // Following 4 functions MUST be called from Scheduler's thread
    void enableReading();
    void disableReading();
    void enableWriting();
    void disableWriting();

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

private:
    IoChannel(int fd, Scheduler * scheduler, TriggerMode mode);

    // Called by Scheduler::process_io_events() when epoll reports events
    void handleIoEvents(uint32_t ev);

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

    template <typename T>
    Task<> performReadImpl(T && funcOrReader)
    {
        co_await scheduler_->run_here();
        while (true)
        {
            if (!readable_)
            {
                co_await ReadableAwaiter{ this };
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
        co_await scheduler_->run_here();
        while (true)
        {
            if (!writable_)
            {
                co_await WritableAwaiter{ this };
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

    inline static std::atomic_uint64_t idSeq_{ 0 };
    const uint64_t id_{ ++idSeq_ };

    int fd_{ -1 };
    Scheduler * scheduler_{ nullptr };
    TriggerMode triggerMode_{ TriggerMode::EdgeTriggered };

    // All members below are accessed only within Scheduler's single thread.
    // No synchronization primitives needed due to serialized execution model.
    uint32_t events_{ 0 };
    bool readable_{ false };
    bool writable_{ true };
    std::coroutine_handle<> readableWaiter_;
    std::coroutine_handle<> writableWaiter_;
};

} // namespace my_coro
