/**
 * @file CoroScheduler.cc
 * @brief Native coroutine scheduler implementation
 */
#include <CoroScheduler.h>
#include <csignal>
#include <cstring>
#include <sys/epoll.h>
#include <sys/eventfd.h>
#include <unistd.h>

namespace my_coro
{

thread_local CoroScheduler * CoroScheduler::current_ = nullptr;

void IoAwaitable::await_suspend(std::coroutine_handle<> h) noexcept
{
    handle_ = h;
    auto * sched = CoroScheduler::current();
    sched->register_io(fd_, op_, h, buf_, len_, &result_);
}

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
    signal(SIGPIPE, SIG_IGN);

    epoll_fd_ = epoll_create1(EPOLL_CLOEXEC);
    wakeup_fd_ = eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);

    epoll_event ev;
    ev.events = EPOLLIN | EPOLLET;
    ev.data.fd = wakeup_fd_;
    epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, wakeup_fd_, &ev);

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

IoAwaitable CoroScheduler::async_read(int fd, void * buf, size_t len)
{
    return IoAwaitable{ fd, buf, len, IoOp::Read };
}

IoAwaitable CoroScheduler::async_write(int fd, const void * buf, size_t len)
{
    return IoAwaitable{ fd, const_cast<void *>(buf), len, IoOp::Write };
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

void CoroScheduler::register_io(int fd, IoOp op, std::coroutine_handle<> coro,
                                void * buf, size_t len, ssize_t * result)
{
    epoll_event ev;
    ev.events = (op == IoOp::Read) ? EPOLLIN : EPOLLOUT;
    ev.events |= EPOLLET | EPOLLONESHOT;
    ev.data.fd = fd;

    if (epoll_fds_.count(fd))
    {
        epoll_ctl(epoll_fd_, EPOLL_CTL_MOD, fd, &ev);
    }
    else
    {
        epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, fd, &ev);
        epoll_fds_.insert(fd);
    }

    io_waiters_[fd] = IoWaiter{ coro, buf, len, result };
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
        int fd = events[i].data.fd;

        if (fd == wakeup_fd_)
        {
            uint64_t val;
            read(wakeup_fd_, &val, sizeof(val));
            continue;
        }

        auto it = io_waiters_.find(fd);
        if (it != io_waiters_.end())
        {
            auto & waiter = it->second;
            ssize_t ret = (events[i].events & EPOLLIN)
                              ? read(fd, waiter.buffer, waiter.size)
                              : write(fd, waiter.buffer, waiter.size);

            *waiter.result = ret;
            ready_queue_.push(waiter.coro);
            io_waiters_.erase(it);

            if (ret <= 0 || (ret < 0 && (errno == EBADF || errno == ECONNRESET || errno == EPIPE)))
            {
                epoll_ctl(epoll_fd_, EPOLL_CTL_DEL, fd, nullptr);
                epoll_fds_.erase(fd);
            }
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

} // namespace my_coro
