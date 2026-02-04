/**
 * @file CoroScheduler.cc
 * @brief Native coroutine scheduler implementation
 */
#include <CoroScheduler.h>
#include <cstring>
#include <sys/epoll.h>
#include <unistd.h>

namespace drogon::coro
{

thread_local CoroScheduler * g_scheduler = nullptr;

void IoAwaitable::await_suspend(std::coroutine_handle<> h) noexcept
{
    handle_ = h;
    auto * sched = current_scheduler();
    sched->register_io(fd_, op_, h, buf_, len_);
}

void TimerAwaitable::await_suspend(std::coroutine_handle<> h) noexcept
{
    handle_ = h;
    auto * sched = current_scheduler();
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
    epoll_fd_ = epoll_create1(EPOLL_CLOEXEC);
    g_scheduler = this;
}

CoroScheduler::~CoroScheduler()
{
    if (epoll_fd_ >= 0)
        close(epoll_fd_);
}

void CoroScheduler::run()
{
    running_.store(true, std::memory_order_release);

    epoll_event events[128];

    while (running_.load(std::memory_order_acquire))
    {
        // 1. 计算超时时间
        int timeout_ms = static_cast<int>(get_next_timeout());

        // 2. 处理 I/O 事件
        int n = epoll_wait(epoll_fd_, events, 128, timeout_ms);

        for (int i = 0; i < n; ++i)
        {
            int fd = events[i].data.fd;
            auto it = io_waiters_.find(fd);
            if (it != io_waiters_.end())
            {
                auto & waiter = it->second;
                ssize_t ret = (events[i].events & EPOLLIN)
                                  ? read(fd, waiter.buffer, waiter.size)
                                  : write(fd, waiter.buffer, waiter.size);

                ready_queue_.push(waiter.coro);
                io_waiters_.erase(it);
            }
        }

        // 3. 处理到期定时器
        process_timers();

        // 4. 恢复就绪协程
        resume_ready_coros();
    }
}

void CoroScheduler::stop()
{
    running_.store(false, std::memory_order_release);
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
}

void CoroScheduler::register_io(int fd, IoOp op, std::coroutine_handle<> coro,
                                void * buf, size_t len)
{
    epoll_event ev;
    ev.events = (op == IoOp::Read) ? EPOLLIN : EPOLLOUT;
    ev.events |= EPOLLET | EPOLLONESHOT;
    ev.data.fd = fd;

    epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, fd, &ev);

    io_waiters_[fd] = IoWaiter{ coro, buf, len };
}

TimerId CoroScheduler::register_timer(TimePoint when, std::coroutine_handle<> coro)
{
    TimerId id = next_timer_id_.fetch_add(1, std::memory_order_relaxed);
    timers_.push(Timer{ id, when, coro });
    return id;
}

void CoroScheduler::process_timers()
{
    auto now = std::chrono::steady_clock::now();

    while (!timers_.empty() && timers_.top().when <= now)
    {
        auto timer = timers_.top();
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

void CoroScheduler::resume_ready_coros()
{
    while (auto coro = ready_queue_.pop())
    {
        if (!coro.done())
            coro.resume();
    }
}

} // namespace drogon::coro
