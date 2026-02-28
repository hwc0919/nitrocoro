/**
 * @file DnsResolver.h
 * @brief Asynchronous DNS resolver using thread pool
 */
#pragma once

#include <nitrocoro/core/Future.h>
#include <nitrocoro/core/Scheduler.h>
#include <nitrocoro/core/Task.h>
#include <nitrocoro/net/InetAddress.h>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <queue>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

namespace nitrocoro::net
{

class DnsResolver
{
public:
    explicit DnsResolver(size_t threadNum = 0,
                         std::chrono::seconds ttl = std::chrono::seconds(60));
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
    using Addresses = std::vector<InetAddress>;
    using TimePoint = std::chrono::steady_clock::time_point;

    struct CacheEntry
    {
        Addresses addresses;
        TimePoint expiry;
    };

    struct ResolveTask
    {
        std::string key;
        std::string hostname;
        std::string service;
        int family;
        std::vector<Promise<Addresses>> waiters;
    };

    struct ExpiryEntry
    {
        TimePoint expiry;
        std::string key;
        bool operator>(const ExpiryEntry & o) const { return expiry > o.expiry; }
    };

    static std::string cacheKey(const std::string & hostname, const std::string & service, int family);
    Task<Addresses> resolveImpl(const std::string & hostname,
                                const std::string & service,
                                int family,
                                Scheduler * scheduler);
    void workerThread();

    std::vector<std::thread> workers_;
    std::queue<ResolveTask> tasks_;
    std::mutex mutex_;
    std::condition_variable cv_;
    bool stop_ = false;
    std::chrono::seconds ttl_;
    std::atomic<uint32_t> writeCount_{ 0 };
    std::unordered_map<std::string, CacheEntry> cache_;
    std::unordered_map<std::string, std::vector<Promise<Addresses>>> pending_;
    std::priority_queue<ExpiryEntry, std::vector<ExpiryEntry>, std::greater<ExpiryEntry>> expiryQueue_;
};

} // namespace nitrocoro::net
