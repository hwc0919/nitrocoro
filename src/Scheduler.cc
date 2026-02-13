/**
 * @file CoroScheduler.cc
 * @brief Native coroutine scheduler implementation
 */
#include <IoChannel.h>
#include <Scheduler.h>
#include <cassert>
#include <csignal>
#include <cstring>
#include <stdexcept>
#include <sys/epoll.h>
#include <sys/eventfd.h>
#include <unistd.h>

namespace my_coro
{

static constexpr int64_t kDefaultTimeoutMs = 10000;

thread_local Scheduler * Scheduler::current_ = nullptr;

Scheduler::Scheduler()
{
    if (current_ != nullptr)
    {
        throw std::logic_error("CoroScheduler already exists in this thread");
    }

    signal(SIGPIPE, SIG_IGN);
    epoll_fd_ = epoll_create1(EPOLL_CLOEXEC);
    if (epoll_fd_ < 0)
    {
        throw std::runtime_error("Failed to create epoll");
    }
    wakeup_fd_ = ::eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
    if (wakeup_fd_ < 0)
    {
        close(epoll_fd_);
        throw std::runtime_error("Failed to create wakeup fd");
    }
    current_ = this;
}

Scheduler::~Scheduler()
{
    if (current_ == this)
        current_ = nullptr;
    if (wakeup_fd_ >= 0)
        close(wakeup_fd_);
    if (epoll_fd_ >= 0)
        close(epoll_fd_);
}

Scheduler * Scheduler::current() noexcept
{
    return current_;
}

void Scheduler::run()
{
    thread_id_ = std::this_thread::get_id();
    wakeupChannel_ = std::make_unique<IoChannel>(wakeup_fd_, this);

    running_.store(true, std::memory_order_release);

    while (running_.load(std::memory_order_acquire))
    {
        int timeout_ms = static_cast<int>(get_next_timeout());
        process_io_events(timeout_ms);
        process_timers();
        resume_ready_coros();
    }
}

void Scheduler::stop()
{
    running_.store(false, std::memory_order_release);
    wakeup();
}

TimerAwaiter Scheduler::sleep_for(double seconds)
{
    auto when = std::chrono::steady_clock::now() + std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::duration<double>(seconds));
    return TimerAwaiter{ this, when };
}

TimerAwaiter Scheduler::sleep_until(TimePoint when)
{
    return TimerAwaiter{ this, when };
}

SchedulerAwaiter Scheduler::run_here() noexcept
{
    return SchedulerAwaiter{ this };
}

void Scheduler::schedule(std::coroutine_handle<> coro)
{
    ready_queue_.push(coro);
    wakeup();
}

void Scheduler::schedule_at(TimePoint when, std::coroutine_handle<> coro)
{
    pending_timers_.push(Timer{ when, coro });
    wakeup();
}

void Scheduler::resume_ready_coros()
{
    while (auto coro = ready_queue_.pop())
    {
        if (!coro->done())
            coro->resume();
    }
}

void Scheduler::process_io_events(int timeout_ms)
{
    epoll_event events[128];
    int n = epoll_wait(epoll_fd_, events, 128, timeout_ms);

    for (int i = 0; i < n; ++i)
    {
        auto * channel = static_cast<IoChannel *>(events[i].data.ptr);
        uint32_t ev = events[i].events;
        if (channel == wakeupChannel_.get())
        {
            uint64_t dummy;
            ssize_t ret = read(wakeup_fd_, &dummy, sizeof(dummy));
            if (ret < 0)
                fprintf(stderr, "wakeup read error: %s\n", strerror(errno));
            continue;
        }

        if (!ioChannels_.contains(channel->fd_) || ioChannels_.at(channel->fd_) != channel)
        {
            printf("channel with fd %d not found!!!\n", channel->fd_);
            continue;
        }

        printf("fd %d event %d: IN: %d, OUT: %d, ERR: %d\n",
               channel->fd_,
               ev,
               ev & EPOLLIN,
               ev & EPOLLOUT,
               ev & (EPOLLERR | EPOLLHUP));
        fflush(stdout);

        if (ev & (EPOLLERR | EPOLLHUP))
        {
            // channel->handleError();
            printf("Unhandled channel error for fd %d\n", channel->fd_);
            continue;
        }

        if (ev & EPOLLIN)
        {
            channel->handleReadable();
        }
        if (ev & EPOLLOUT)
        {
            printf("handleWritable OUT: %d\n", ev & EPOLLOUT);
            channel->handleWritable();
        }
    }
}

int64_t Scheduler::get_next_timeout()
{
    while (auto timer = pending_timers_.pop())
    {
        timers_.push(std::move(*timer));
    }

    if (timers_.empty())
        return kDefaultTimeoutMs;

    auto now = std::chrono::steady_clock::now();
    auto next = timers_.top().when;

    if (next <= now)
        return 0;

    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(next - now);
    return ms.count();
}

void Scheduler::process_timers()
{
    if (timers_.empty())
        return;

    auto now = std::chrono::steady_clock::now();

    while (!timers_.empty() && timers_.top().when <= now)
    {
        auto timer = std::move(timers_.top());
        timers_.pop();
        ready_queue_.push(timer.coro);
    }
}

void Scheduler::wakeup()
{
    uint64_t val = 1;
    write(wakeup_fd_, &val, sizeof(val));
}

void Scheduler::registerIoChannel(IoChannel * channel)
{
    int fd = channel->fd_;
    epoll_event ev{};
    ev.events = channel->events_ | (channel->triggerMode_ == TriggerMode::EdgeTriggered ? EPOLLET : 0);
    ev.data.ptr = channel;

    assert(!ioChannels_.contains(fd));
    ioChannels_[fd] = channel;

    if (::epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, channel->fd_, &ev) < 0)
    {
        throw std::runtime_error("Failed to call EPOLL_CTL_ADD on epoll");
    }
}

void Scheduler::unregisterIoChannel(IoChannel * channel)
{
    int fd = channel->fd_;
    assert(ioChannels_.contains(fd));
    assert(ioChannels_.at(fd) == channel);
    size_t n = ioChannels_.erase(fd);
    (void)n;
    assert(n == 1);

    epoll_event ev{};
    ev.events = 0;
    ev.data.ptr = channel;
    if (::epoll_ctl(epoll_fd_, EPOLL_CTL_DEL, fd, &ev) < 0)
    {
        printf("Failed to call EPOLL_CTL_DEL on epoll %d fd %d, error = %d", epoll_fd_, fd, errno);
        // throw std::runtime_error("Failed to call EPOLL_CTL_DEL on epoll");
    }
}

void Scheduler::updateChannel(IoChannel * channel)
{
    epoll_event ev{};
    ev.events = channel->events_ | (channel->triggerMode_ == TriggerMode::EdgeTriggered ? EPOLLET : 0);
    ev.data.ptr = channel;
    ::epoll_ctl(epoll_fd_, EPOLL_CTL_MOD, channel->fd_, &ev);
}

bool Scheduler::isInOwnThread() const noexcept
{
    return std::this_thread::get_id() == thread_id_;
}

void TimerAwaiter::await_suspend(std::coroutine_handle<> h) noexcept
{
    sched_->schedule_at(when_, h);
}

bool SchedulerAwaiter::await_ready() const noexcept
{
    return scheduler_->isInOwnThread();
}

void SchedulerAwaiter::await_suspend(std::coroutine_handle<> h) noexcept
{
    scheduler_->schedule(h);
}

} // namespace my_coro
