/**
 * @file DnsResolver.h
 * @brief Asynchronous DNS resolver using thread pool
 */
#pragma once

#include <nitrocoro/core/Future.h>
#include <nitrocoro/core/Scheduler.h>
#include <nitrocoro/core/Task.h>
#include <nitrocoro/net/InetAddress.h>

#include <condition_variable>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

namespace nitrocoro::net
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

} // namespace nitrocoro::net
