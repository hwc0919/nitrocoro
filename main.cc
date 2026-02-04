#include "CoroScheduler.h"
#include <iostream>

using namespace drogon::coro;

struct Task
{
    struct promise_type
    {
        Task get_return_object() { return {}; }
        std::suspend_never initial_suspend() { return {}; }
        std::suspend_never final_suspend() noexcept { return {}; }
        void return_void() {}
        void unhandled_exception() {}
    };
};

Task example_task(CoroScheduler & scheduler)
{
    std::cout << "Task started\n";
    co_await scheduler.sleep_for(1.0);
    std::cout << "Task after 1 second\n";
    co_await scheduler.sleep_for(0.5);
    std::cout << "Task after 1.5 seconds\n";
}

Task example_task2(CoroScheduler & scheduler)
{
    std::cout << "Task2 started\n";
    co_await scheduler.sleep_for(1.0);
    std::cout << "Task2 after 1 second\n";
    co_await scheduler.sleep_for(0.5);
    std::cout << "Task2 after 1.5 seconds\n";
}

int main()
{
    CoroScheduler scheduler;
    example_task(scheduler);
    example_task2(scheduler);
    scheduler.run();
    return 0;
}