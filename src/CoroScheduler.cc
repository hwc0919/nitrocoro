/**
 * @file CoroScheduler.cc
 * @brief Native coroutine scheduler implementation
 */
#include <CoroScheduler.h>
#include <IoChannel.h>
#include <cassert>
#include <csignal>
#include <cstring>
#include <stdexcept>
#include <sys/epoll.h>
#include <sys/eventfd.h>
#include <unistd.h>

namespace my_coro
{

thread_local CoroScheduler * CoroScheduler::current_ = nullptr;

void TimerAwaitable::await_suspend(std::coroutine_handle<> h) noexcept
{
    handle_ = h;
    auto * sched = CoroScheduler::current();
    sched->register_timer(when_, h);
}

void CoroScheduler::ReadyQueue::push(std::coroutine_handle<> h)
{
    size_t t = tail_.load(std::memory_order_relaxed);
    coros[t % 1024] = h;
    tail_.store(t + 1, std::memory_order_release);
}

std::coroutine_handle<> CoroScheduler::ReadyQueue::pop()
{
    size_t h = head_.load(std::memory_order_relaxed);
    size_t t = tail_.load(std::memory_order_acquire);
    if (h >= t)
        return nullptr;
    auto coro = coros[h % 1024];
    head_.store(h + 1, std::memory_order_release);
    return coro;
}

CoroScheduler::CoroScheduler()
{
    if (current_ != nullptr)
    {
        throw std::logic_error("CoroScheduler already exists in this thread");
    }

    signal(SIGPIPE, SIG_IGN);
    epoll_fd_ = epoll_create1(EPOLL_CLOEXEC);
    current_ = this;
}

CoroScheduler::~CoroScheduler()
{
    if (current_ == this)
        current_ = nullptr;
    if (wakeup_fd_ >= 0)
        close(wakeup_fd_);
    if (epoll_fd_ >= 0)
        close(epoll_fd_);
}

CoroScheduler * CoroScheduler::current() noexcept
{
    return current_;
}

void CoroScheduler::run()
{
    // 创建 wakeup_channel_
    wakeup_fd_ = eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
    wakeupChannel_ = std::make_unique<IoChannel>(wakeup_fd_, this);

    running_.store(true, std::memory_order_release);

    while (running_.load(std::memory_order_acquire))
    {
        resume_ready_coros();
        process_io_events();
        process_timers();
    }
}

void CoroScheduler::stop()
{
    running_.store(false, std::memory_order_release);
    wakeup();
}

TimerAwaitable CoroScheduler::sleep_for(double seconds)
{
    auto when = std::chrono::steady_clock::now() + std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::duration<double>(seconds));
    return TimerAwaitable{ when };
}

TimerAwaitable CoroScheduler::sleep_until(TimePoint when)
{
    return TimerAwaitable{ when };
}

void CoroScheduler::schedule(std::coroutine_handle<> coro)
{
    ready_queue_.push(coro);
    wakeup();
}

TimerId CoroScheduler::register_timer(TimePoint when, std::coroutine_handle<> coro)
{
    TimerId id = next_timer_id_.fetch_add(1, std::memory_order_relaxed);
    timers_.push(Timer{ id, when, coro });
    return id;
}

void CoroScheduler::resume_ready_coros()
{
    while (auto coro = ready_queue_.pop())
    {
        if (!coro.done())
            coro.resume();
    }
}

void CoroScheduler::process_io_events()
{
    int timeout_ms = static_cast<int>(get_next_timeout());
    epoll_event events[128];
    int n = epoll_wait(epoll_fd_, events, 128, timeout_ms);

    for (int i = 0; i < n; ++i)
    {
        IoChannel * channel = static_cast<IoChannel *>(events[i].data.ptr);
        uint32_t ev = events[i].events;
        if (channel == wakeupChannel_.get())
        {
            char buf[8];
            while (::read(wakeup_fd_, buf, sizeof(buf)) > 0)
            {
                //
            }
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
        // 处理写事件
        if (ev & EPOLLOUT)
        {
            printf("handleWritable OUT: %d\n", ev & EPOLLOUT);
            channel->handleWritable();
        }
    }
}

void CoroScheduler::process_timers()
{
    auto now = std::chrono::steady_clock::now();

    while (!timers_.empty() && timers_.top().when <= now)
    {
        auto timer = std::move(timers_.top());
        timers_.pop();
        ready_queue_.push(timer.coro);
    }
}

int64_t CoroScheduler::get_next_timeout() const
{
    if (timers_.empty())
        return 10000; // 10秒默认超时

    auto now = std::chrono::steady_clock::now();
    auto next = timers_.top().when;

    if (next <= now)
        return 0;

    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(next - now);
    return ms.count();
}

void CoroScheduler::wakeup()
{
    uint64_t val = 1;
    write(wakeup_fd_, &val, sizeof(val));
}

void CoroScheduler::registerIoChannel(IoChannel * channel)
{
    int fd = channel->fd_;
    epoll_event ev{};
    ev.events = EPOLLIN | EPOLLOUT | EPOLLET; // PRI?
    ev.data.ptr = channel;

    assert(!ioChannels_.contains(fd));
    ioChannels_[fd] = channel;

    if (::epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, channel->fd_, &ev) < 0)
    {
        throw std::runtime_error("Failed to call EPOLL_CTL_ADD on epoll");
    }
}

void CoroScheduler::unregisterIoChannel(IoChannel * channel)
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

} // namespace my_coro
