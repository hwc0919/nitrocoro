/**
 * @file CoroScheduler.cc
 * @brief Native coroutine scheduler implementation
 */
#include <cassert>
#include <csignal>
#include <cstring>
#include <nitro_coro/core/Scheduler.h>
#include <nitro_coro/io/IoChannel.h>
#include <stdexcept>
#include <sys/epoll.h>
#include <sys/eventfd.h>
#include <unistd.h>

#define NITRO_CORO_SCHEDULER_ASSERT_IN_OWN_THREAD() \
    assert(isInOwnThread() && "Must be called in its own thread")

namespace nitro_coro
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
    wakeupChannel_ = io::IoChannel::create(wakeup_fd_, this);
    wakeupChannel_->enableReading();

    running_.store(true, std::memory_order_release);

    while (running_.load(std::memory_order_acquire))
    {
        int timeout_ms = static_cast<int>(get_next_timeout());
        process_io_events(timeout_ms);
        process_timers();
        process_ready_queue();
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

void Scheduler::schedule(std::coroutine_handle<> handle)
{
    ready_queue_.push([handle]() { handle.resume(); });
    wakeup();
}

void Scheduler::schedule_at(TimePoint when, std::coroutine_handle<> handle)
{
    pending_timers_.push(Timer{ when, handle });
    wakeup();
}

void Scheduler::process_ready_queue()
{
    while (auto func = ready_queue_.pop())
    {
        (*func)();
    }
}

void Scheduler::process_io_events(int timeout_ms)
{
    epoll_event events[128];
    // TODO: abstract poller
    int n = epoll_wait(epoll_fd_, events, 128, timeout_ms);

    for (int i = 0; i < n; ++i)
    {
        uint64_t channelId = events[i].data.u64;
        uint32_t ev = events[i].events;
        if (channelId == wakeupChannel_->id())
        {
            uint64_t dummy;
            ssize_t ret = read(wakeup_fd_, &dummy, sizeof(dummy));
            if (ret < 0)
                fprintf(stderr, "wakeup read error: %s\n", strerror(errno));
            continue;
        }

        auto iter = ioChannels_.find(channelId);
        if (iter == ioChannels_.end())
        {
            printf("channel with id %ld not found!!!\n", channelId);
            continue;
        }
        auto * ctx = &iter->second;
        int fd = ctx->fd;

        printf("fd %d event %d: IN: %d, OUT: %d, ERR: %d\n",
               fd,
               ev,
               ev & EPOLLIN,
               ev & EPOLLOUT,
               ev & (EPOLLERR | EPOLLHUP));
        fflush(stdout);

        ctx->handler(fd, ev);
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
        ready_queue_.push([handle = timer.handle]() { handle.resume(); });
    }
}

void Scheduler::wakeup()
{
    uint64_t val = 1;
    write(wakeup_fd_, &val, sizeof(val));
}

void Scheduler::setIoChannelHandler(const std::shared_ptr<io::IoChannel> & channel, Scheduler::IoEventHandler handler)
{
    NITRO_CORO_SCHEDULER_ASSERT_IN_OWN_THREAD();

    uint64_t id = channel->id();
    int fd = channel->fd();
    auto iter = ioChannels_.find(id);
    if (iter != ioChannels_.end())
    {
        assert(iter->second.handler == nullptr);
        iter->second.handler = std::move(handler);
    }
    else
    {
        ioChannels_.emplace(id, IoChannelContext{ id, fd, { channel }, std::move(handler) });
    }
}

void Scheduler::updateIoChannel(const std::shared_ptr<io::IoChannel> & channel)
{
    NITRO_CORO_SCHEDULER_ASSERT_IN_OWN_THREAD();

    uint64_t id = channel->id();
    int fd = channel->fd();
    IoChannelContext * ctx;
    auto iter = ioChannels_.find(id);
    if (iter != ioChannels_.end())
    {
        ctx = &iter->second;
        assert(ctx->weakChannel.lock() == channel);
    }
    else
    {
        ctx = &ioChannels_.emplace(id, IoChannelContext{ id, fd, { channel }, IoEventHandler{} }).first->second;
    }

    uint32_t events = channel->events();
    if (events == 0)
    {
        if (ctx->addedToEpoll)
        {
            epoll_event ev{};
            if (::epoll_ctl(epoll_fd_, EPOLL_CTL_DEL, fd, &ev) < 0)
            {
                throw std::runtime_error("Failed to call EPOLL_CTL_DEL on epoll");
            }
            ctx->addedToEpoll = false;
        }
        return;
    }

    epoll_event ev{};
    ev.events = events | (channel->triggerMode() == io::TriggerMode::EdgeTriggered ? EPOLLET : 0);
    ev.data.u64 = id;

    int op = ctx->addedToEpoll ? EPOLL_CTL_MOD : EPOLL_CTL_ADD;
    if (::epoll_ctl(epoll_fd_, op, fd, &ev) < 0)
    {
        throw std::runtime_error(ctx->addedToEpoll ? "Failed to call EPOLL_CTL_MOD on epoll" : "Failed to call EPOLL_CTL_ADD on epoll");
    }

    ctx->addedToEpoll = true;
}

void Scheduler::removeIoChannel(uint64_t id)
{
    NITRO_CORO_SCHEDULER_ASSERT_IN_OWN_THREAD();

    assert(ioChannels_.contains(id));
    int fd = ioChannels_.at(id).fd;
    ioChannels_.erase(id);

    epoll_event ev{};
    ev.events = 0;
    ev.data.u64 = id;
    if (::epoll_ctl(epoll_fd_, EPOLL_CTL_DEL, fd, &ev) < 0)
    {
        printf("Failed to call EPOLL_CTL_DEL on epoll %d fd %d, error = %d", epoll_fd_, fd, errno);
        // throw std::runtime_error("Failed to call EPOLL_CTL_DEL on epoll");
    }
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

} // namespace nitro_coro
