#include <nitrocoro/core/Scheduler.h>
#include <nitrocoro/core/Task.h>
#include <atomic>
#include <iostream>
#include <memory>

using namespace nitrocoro;

Task<> example_task(int i, int n)
{
    auto * scheduler = Scheduler::current();
    std::cout << "Task " << i << " started\n";
    co_await scheduler->sleep_for(1.0);
    std::cout << "Task " << i << " after 1 second\n";
    co_await scheduler->sleep_for(0.5);
    std::cout << "Task " << i << " after 1.5 seconds\n";

    if (i < n)
    {
        co_await example_task(i + 1, n);
    }
}

Task<> main_coro()
{
    auto cnt = std::make_shared<std::atomic_int>(2);
    Scheduler::current()->spawn([cnt]() -> Task<> {
        co_await example_task(101, 102);
        if (--*cnt == 0)
        {
            std::cout << "stop scheduler\n";
            Scheduler::current()->stop();
        }
    });

    co_await example_task(1, 2);
    if (--*cnt == 0)
    {
        std::cout << "stop scheduler\n";
        Scheduler::current()->stop();
    }
}

int main()
{
    std::cout << "Hello, World!\n";
    Scheduler scheduler;
    scheduler.spawn(main_coro);
    scheduler.run();
    std::cout << "Goodbye, World!\n";
    return 0;
}
