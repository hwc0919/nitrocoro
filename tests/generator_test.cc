#include <iostream>
#include <nitrocoro/core/Generator.h>
#include <nitrocoro/core/Scheduler.h>
#include <nitrocoro/core/Task.h>

using namespace nitrocoro;

// 同步 Generator
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

// 异步获取数据
Task<int> fetch_data(int id)
{
    std::cout << "  [Fetching " << id << "...]\n";
    co_await Scheduler::current()->sleep_for(0.3);
    std::cout << "  [Got " << id << "]\n";
    co_return id * 10;
}

// Generator 产出 Task - 异步数据流
Generator<Task<int>> async_data_stream(int count)
{
    for (int i = 1; i <= count; ++i)
        co_yield fetch_data(i);
}

Task<> main_coro()
{
    // 1. 同步 Generator
    std::cout << "=== Sync Generator ===\n";
    std::cout << "Range: ";
    for (int n : range(0, 10))
        std::cout << n << " ";
    std::cout << "\n";

    std::cout << "Fibonacci: ";
    for (int n : fibonacci(10))
        std::cout << n << " ";
    std::cout << "\n\n";

    // 2. Generator 产出 Task - 异步数据流
    std::cout << "=== Async Data Stream ===\n";
    int sum = 0;
    for (Task<int> & task : async_data_stream(5))
    {
        int value = co_await std::move(task); // 移动 Task
        std::cout << "Received: " << value << "\n";
        sum += value;
    }
    std::cout << "Total: " << sum << "\n";

    Scheduler::current()->stop();
}

int main()
{
    Scheduler scheduler;
    scheduler.spawn(main_coro);
    scheduler.run();
    return 0;
}
