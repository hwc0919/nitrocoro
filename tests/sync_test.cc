/**
 * @file sync_test.cc
 * @brief Tests for Future/Promise/SharedFuture and Mutex.
 */
#include <nitrocoro/core/Future.h>
#include <nitrocoro/core/Mutex.h>
#include <nitrocoro/core/Scheduler.h>
#include <nitrocoro/core/Task.h>
#include <nitrocoro/testing/Test.h>

using namespace nitrocoro;

// ── Future / Promise ──────────────────────────────────────────────────────────

/** A value set by Promise is received by the awaiting Future. */
NITRO_TEST(future_set_value)
{
    Promise<int> p(Scheduler::current());
    auto f = p.get_future();
    Scheduler::current()->spawn([TEST_CTX, p = std::move(p)]() mutable -> Task<> {
        co_await Scheduler::current()->sleep_for(0.02);
        p.set_value(99);
    });
    NITRO_CHECK_EQ(co_await f.get(), 99);
    co_return;
}

/** An exception set on a Promise is rethrown when the Future is awaited. */
NITRO_TEST(future_set_exception)
{
    Promise<int> p(Scheduler::current());
    auto f = p.get_future();
    Scheduler::current()->spawn([TEST_CTX, p = std::move(p)]() mutable -> Task<> {
        p.set_exception(std::make_exception_ptr(std::runtime_error("err")));
        co_return;
    });
    NITRO_CHECK_THROWS_AS(co_await f.get(), std::runtime_error);
}

/** SharedFuture resumes all waiters when the Promise is fulfilled. */
NITRO_TEST(shared_future_multiple_waiters)
{
    Promise<int> p(Scheduler::current());
    auto sf = p.get_future().share();
    int sum = 0;

    Scheduler::current()->spawn([TEST_CTX, sf, &sum]() mutable -> Task<> {
        sum += co_await sf.get();
    });
    Scheduler::current()->spawn([TEST_CTX, sf, &sum]() mutable -> Task<> {
        sum += co_await sf.get();
    });

    co_await Scheduler::current()->sleep_for(0.01);
    p.set_value(10);
    co_await Scheduler::current()->sleep_for(0.01);
    NITRO_CHECK_EQ(sum, 20);
}

// ── Mutex ─────────────────────────────────────────────────────────────────────

/** try_lock succeeds when unlocked and fails when already held. */
NITRO_TEST(mutex_try_lock)
{
    Mutex m;
    NITRO_CHECK(m.try_lock());
    NITRO_CHECK(!m.try_lock());
    m.unlock();
    NITRO_CHECK(m.try_lock());
    m.unlock();
    co_return;
}

/** scoped_lock provides exclusive access; concurrent coroutines serialize. */
NITRO_TEST(mutex_scoped_lock_exclusive)
{
    Mutex m;
    int counter = 0;
    Promise<> done(Scheduler::current());
    auto f = done.get_future();

    Scheduler::current()->spawn([TEST_CTX, &m, &counter, done = std::move(done)]() mutable -> Task<> {
        for (int i = 0; i < 5; ++i)
        {
            [[maybe_unused]] auto lock = co_await m.scoped_lock();
            ++counter;
        }
        done.set_value();
    });

    for (int i = 0; i < 5; ++i)
    {
        [[maybe_unused]] auto lock = co_await m.scoped_lock();
        ++counter;
    }
    co_await f.get();
    NITRO_CHECK_EQ(counter, 10);
}

int main()
{
    return nitrocoro::test::run_all();
}
