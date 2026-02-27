#include <nitrocoro/core/Generator.h>
#include <nitrocoro/core/Scheduler.h>
#include <nitrocoro/core/Task.h>
#include <nitrocoro/testing/Test.h>

using namespace nitrocoro;

Generator<int> range(int start, int end)
{
    for (int i = start; i < end; ++i)
        co_yield i;
}

Generator<int> fibonacci(int n)
{
    int a = 0, b = 1;
    for (int i = 0; i < n; ++i)
    {
        co_yield a;
        auto next = a + b;
        a = b;
        b = next;
    }
}

Task<int> fetch_data(int id)
{
    co_await Scheduler::current()->sleep_for(0.05);
    co_return id * 10;
}

// ── Tests ─────────────────────────────────────────────────────────────────────

NITRO_TEST(range_sum)
{
    int sum = 0;
    for (int n : range(0, 5))
        sum += n;
    NITRO_CHECK_EQ(sum, 10); // 0+1+2+3+4
    co_return;
}

NITRO_TEST(fibonacci_sequence)
{
    std::vector<int> expected{ 0, 1, 1, 2, 3, 5, 8, 13, 21, 34 };
    std::vector<int> got;
    for (int n : fibonacci(10))
        got.push_back(n);
    NITRO_REQUIRE_EQ(got.size(), expected.size());
    for (size_t i = 0; i < expected.size(); ++i)
        NITRO_CHECK_EQ(got[i], expected[i]);
    co_return;
}

NITRO_TEST(async_fetch)
{
    int total = 0;
    for (int i = 1; i <= 3; ++i)
        total += co_await fetch_data(i);
    NITRO_CHECK_EQ(total, 60); // 10+20+30
}

NITRO_TEST(spawn_check)
{
    // Capture TEST_CTX so the test waits for the spawned coroutine.
    Scheduler::current()->spawn([TEST_CTX]() -> Task<> {
        int val = co_await fetch_data(7);
        NITRO_CHECK_EQ(val, 70);
    });
    co_return;
}

NITRO_TEST_EXPECT_FAIL(intentional_check_failure)
{
    NITRO_CHECK_EQ(1, 2);
    NITRO_CHECK(true); // still runs after CHECK failure
    co_return;
}

NITRO_TEST_EXPECT_FAIL(require_aborts_early)
{
    NITRO_REQUIRE_EQ(1, 2); // fails → co_return, next line must NOT run
    NITRO_CHECK(false);
    co_return;
}

int main()
{
    return nitrocoro::test::run_all();
}
