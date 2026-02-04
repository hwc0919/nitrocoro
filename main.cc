#include "CoroScheduler.h"
#include "Task.h"
#include <atomic>
#include <iostream>
#include <memory>

using namespace my_coro;

Task<> example_task(int i, int n)
{
    auto & scheduler = *current_scheduler();
    std::cout << "Task " << i << " started\n";
    co_await scheduler.sleep_for(1.0);
    std::cout << "Task " << i << " after 1 second\n";
    co_await scheduler.sleep_for(0.5);
    std::cout << "Task " << i << " after 1.5 seconds\n";

    if (i < n)
    {
        co_await example_task(i + 1, n);
    }
}

Task<> main_coro()
{
    auto cnt = std::make_shared<std::atomic_int>(2);
    current_scheduler()->spawn([cnt]() -> Task<> {
        co_await example_task(101, 102);
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
