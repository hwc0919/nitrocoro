/**
 * @file DnsResolver.h
 * @brief Asynchronous DNS resolver using thread pool
 */
#pragma once

#include <nitro_coro/core/Future.h>
#include <nitro_coro/core/Scheduler.h>
#include <nitro_coro/core/Task.h>
#include <nitro_coro/net/InetAddress.h>

#include <condition_variable>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

namespace nitro_coro::net
{

class DnsResolver
{
public:
    explicit DnsResolver(size_t threadNum = 0);
    ~DnsResolver();

    DnsResolver(const DnsResolver &) = delete;
    DnsResolver & operator=(const DnsResolver &) = delete;

    Task<std::vector<InetAddress>> resolve(const std::string & hostname,
                                           const std::string & service = "",
                                           Scheduler * scheduler = Scheduler::current());
    Task<std::vector<InetAddress>> resolve(const std::string & hostname,
                                           int family,
                                           Scheduler * scheduler = Scheduler::current());

private:
    struct ResolveTask
    {
        std::string hostname;
        std::string service;
        int family;
        Promise<std::vector<InetAddress>> promise;
    };

    void workerThread();

    std::vector<std::thread> workers_;
    std::queue<ResolveTask> tasks_;
    std::mutex mutex_;
    std::condition_variable cv_;
    bool stop_ = false;
};

} // namespace nitro_coro::net
