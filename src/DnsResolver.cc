/**
 * @file DnsResolver.cc
 * @brief Asynchronous DNS resolver implementation
 */
#include <nitro_coro/net/DnsException.h>
#include <nitro_coro/net/DnsResolver.h>

#include <algorithm>
#include <thread>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/socket.h>
#endif

namespace nitro_coro::net
{

DnsResolver::DnsResolver(size_t threadNum)
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

Task<std::vector<InetAddress>> DnsResolver::resolve(const std::string & hostname,
                                                    const std::string & service,
                                                    Scheduler * scheduler)
{
    Promise<std::vector<InetAddress>> promise(scheduler);
    auto future = promise.get_future();

    {
        std::lock_guard lock(mutex_);
        tasks_.push({ hostname, service, AF_UNSPEC, std::move(promise) });
    }
    cv_.notify_one();

    co_return co_await future.get();
}

Task<std::vector<InetAddress>> DnsResolver::resolve(const std::string & hostname,
                                                    int family,
                                                    Scheduler * scheduler)
{
    Promise<std::vector<InetAddress>> promise(scheduler);
    auto future = promise.get_future();

    {
        std::lock_guard lock(mutex_);
        tasks_.push({ hostname, "", family, std::move(promise) });
    }
    cv_.notify_one();

    co_return co_await future.get();
}

void DnsResolver::workerThread()
{
    while (true)
    {
        ResolveTask task{ "", "", AF_UNSPEC, Promise<std::vector<InetAddress>>(nullptr) };

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

        struct addrinfo hints = {};
        hints.ai_family = task.family;
        hints.ai_socktype = SOCK_STREAM;

        struct addrinfo * res = nullptr;
        int error = getaddrinfo(task.hostname.c_str(),
                                task.service.empty() ? nullptr : task.service.c_str(),
                                &hints,
                                &res);

        if (error == 0)
        {
            std::vector<InetAddress> addresses;
            for (struct addrinfo * p = res; p != nullptr; p = p->ai_next)
            {
                if (p->ai_family == AF_INET)
                {
                    addresses.emplace_back(*reinterpret_cast<struct sockaddr_in *>(p->ai_addr));
                }
                else if (p->ai_family == AF_INET6)
                {
                    addresses.emplace_back(*reinterpret_cast<struct sockaddr_in6 *>(p->ai_addr));
                }
            }
            freeaddrinfo(res);
            task.promise.set_value(std::move(addresses));
        }
        else
        {
#ifdef _WIN32
            const char * msg = gai_strerrorA(error);
#else
            const char * msg = gai_strerror(error);
#endif
            task.promise.set_exception(std::make_exception_ptr(DnsException(msg, error)));
        }
    }
}

} // namespace nitro_coro::net
