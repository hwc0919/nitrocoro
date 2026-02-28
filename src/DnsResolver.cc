/**
 * @file DnsResolver.cc
 * @brief Asynchronous DNS resolver implementation
 */
#include <nitrocoro/net/DnsException.h>
#include <nitrocoro/net/DnsResolver.h>

#include <algorithm>
#include <thread>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <netdb.h>
#include <sys/socket.h>
#endif

namespace nitrocoro::net
{

DnsResolver::DnsResolver(size_t threadNum, std::chrono::seconds ttl)
    : ttl_(ttl)
{
    if (threadNum == 0)
    {
        threadNum = std::clamp(std::thread::hardware_concurrency(), 1u, 8u);
    }

    for (size_t i = 0; i < threadNum; ++i)
    {
        workers_.emplace_back([this] { workerThread(); });
    }
}

DnsResolver::~DnsResolver()
{
    {
        std::lock_guard lock(mutex_);
        stop_ = true;
    }
    cv_.notify_all();

    for (auto & worker : workers_)
    {
        if (worker.joinable())
            worker.join();
    }
}

std::string DnsResolver::cacheKey(const std::string & hostname, const std::string & service, int family)
{
    return hostname + "|" + service + "|" + std::to_string(family);
}

Task<DnsResolver::Addresses> DnsResolver::resolveImpl(const std::string & hostname,
                                                      const std::string & service,
                                                      int family,
                                                      Scheduler * scheduler)
{
    const std::string key = cacheKey(hostname, service, family);

    Promise<Addresses> promise(scheduler);
    auto future = promise.get_future();
    bool notify = false;
    auto now = std::chrono::steady_clock::now();

    {
        std::lock_guard lock(mutex_);

        auto cacheIt = cache_.find(key);
        if (cacheIt != cache_.end() && now < cacheIt->second.expiry)
        {
            promise.set_value(cacheIt->second.addresses);
        }
        else if (auto pendingIt = pending_.find(key); pendingIt != pending_.end())
        {
            pendingIt->second.push_back(std::move(promise));
        }
        else
        {
            pending_[key].push_back(std::move(promise));
            tasks_.push({ key, hostname, service, family, {} });
            notify = true;
        }
    }

    if (notify)
        cv_.notify_one();

    co_return co_await future.get();
}

Task<DnsResolver::Addresses> DnsResolver::resolve(const std::string & hostname,
                                                  const std::string & service,
                                                  Scheduler * scheduler)
{
    co_return co_await resolveImpl(hostname, service, AF_UNSPEC, scheduler);
}

Task<DnsResolver::Addresses> DnsResolver::resolve(const std::string & hostname,
                                                  int family,
                                                  Scheduler * scheduler)
{
    co_return co_await resolveImpl(hostname, "", family, scheduler);
}

void DnsResolver::workerThread()
{
    while (true)
    {
        ResolveTask task{ "", "", "", AF_UNSPEC, {} };

        {
            std::unique_lock lock(mutex_);
            cv_.wait(lock, [this] { return stop_ || !tasks_.empty(); });

            if (stop_)
                return;

            if (!tasks_.empty())
            {
                task = std::move(tasks_.front());
                tasks_.pop();
            }
            else
            {
                continue;
            }
        }

        std::exception_ptr ex;
        std::vector<Promise<Addresses>> waiters;

        struct addrinfo hints = {};
        hints.ai_family = task.family;
        hints.ai_socktype = SOCK_STREAM;

        struct addrinfo * res = nullptr;
        int error = getaddrinfo(task.hostname.c_str(),
                                task.service.empty() ? nullptr : task.service.c_str(),
                                &hints,
                                &res);

        do
        {
            if (!res)
            {
                ex = std::make_exception_ptr(DnsException("no result", error));
                break;
            }
            if (error != 0)
            {

                freeaddrinfo(res);
#ifdef _WIN32
                ex = std::make_exception_ptr(DnsException(gai_strerrorA(error), error));
#else
                ex = std::make_exception_ptr(DnsException(gai_strerror(error), error));
#endif
                break;
            }

            Addresses addresses;
            for (struct addrinfo * p = res; p != nullptr; p = p->ai_next)
            {
                if (p->ai_family == AF_INET && p->ai_addr)
                    addresses.emplace_back(*reinterpret_cast<struct sockaddr_in *>(p->ai_addr));
                else if (p->ai_family == AF_INET6 && p->ai_addr)
                    addresses.emplace_back(*reinterpret_cast<struct sockaddr_in6 *>(p->ai_addr));
            }
            freeaddrinfo(res);

            if (addresses.empty())
            {
                ex = std::make_exception_ptr(DnsException("no usable addresses", 0));
                break;
            }

            auto now = std::chrono::steady_clock::now();
            auto expiry = now + ttl_;
            {
                std::lock_guard lock(mutex_);
                cache_[task.key] = { addresses, expiry };
                expiryQueue_.push({ expiry, task.key });
                auto pendingIt = pending_.find(task.key);
                if (pendingIt != pending_.end())
                {
                    waiters = std::move(pendingIt->second);
                    pending_.erase(pendingIt);
                }
            }
            if ((writeCount_.fetch_add(1, std::memory_order_relaxed) & 15) == 0)
            {
                std::lock_guard lock(mutex_);
                while (!expiryQueue_.empty() && expiryQueue_.top().expiry <= now)
                {
                    auto & top = expiryQueue_.top();
                    auto cacheIt = cache_.find(top.key);
                    if (cacheIt != cache_.end() && cacheIt->second.expiry <= now)
                        cache_.erase(cacheIt);
                    expiryQueue_.pop();
                }
            }
            for (auto & p : waiters)
                p.set_value(addresses);
        } while (0);

        if (ex)
        {
            {
                std::lock_guard lock(mutex_);
                auto pendingIt = pending_.find(task.key);
                if (pendingIt != pending_.end())
                {
                    waiters = std::move(pendingIt->second);
                    pending_.erase(pendingIt);
                }
            }
            for (auto & p : waiters)
                p.set_exception(ex);
        }
    }
}

} // namespace nitrocoro::net
