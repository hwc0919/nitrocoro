#include "CoroScheduler.h"
#include <atomic>
#include <iostream>
#include <memory>

using namespace drogon::coro;

Task example_task(int i, int n)
{
    auto & scheduler = *current_scheduler();
    std::cout << "Task " << i << " started\n";
    co_await scheduler.sleep_for(1.0);
    std::cout << "Task " << i << " after 1 second\n";
    co_await scheduler.sleep_for(0.5);
    std::cout << "Task " << i << " after 1.5 seconds\n";

    if (i < n)
    {
        std::cout << "Task " << i << " spawning task " << (i + 1) << "\n";
        co_await example_task(i + 1, n);
        std::cout << "Task " << i << " resumed after task " << (i + 1) << " finished\n";
    }
}

Task main_coro()
{
    auto cnt = std::make_shared<std::atomic_int>(2);
    std::cout << "cnt use_count: " << cnt.use_count() << "\n"; // 应该 = 2

    current_scheduler()->spawn([cnt]() -> Task {
        std::cout << "cnt use_count: " << cnt.use_count() << "\n"; // 应该 >= 2

        std::cout << "Spawned task\n";
        co_await example_task(101, 102);
        std::cout << "Spawned task finished\n";
        std::cout << "cnt use_count: " << cnt.use_count() << "\n"; // 应该 >= 1
        if (--*cnt == 0)
        {
            std::cout << "stop scheduler\n";
            current_scheduler()->stop();
        }
    });

    co_await example_task(1, 2);
    if (--*cnt == 0)
    {
        std::cout << "stop scheduler\n";
        current_scheduler()->stop();
    }
}

int main()
{
    std::cout << "Hello, World!\n";
    CoroScheduler scheduler;
    scheduler.spawn(main_coro);
    scheduler.run();
    std::cout << "Goodbye, World!\n";
    return 0;
}
