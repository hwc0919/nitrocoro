/**
 * @file DnsResolver.cc
 * @brief Asynchronous DNS resolver implementation
 */
#include <nitrocoro/core/Future.h>
#include <nitrocoro/net/DnsResolver.h>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <netdb.h>
#include <sys/socket.h>
#endif

namespace nitrocoro::net
{

using Addresses = std::vector<InetAddress>;
using TimePoint = std::chrono::steady_clock::time_point;

struct CacheEntry
{
    Addresses addresses;
    TimePoint expiry;
};

struct ExpiryEntry
{
    TimePoint expiry;
    std::string key;
    bool operator>(const ExpiryEntry & o) const { return expiry > o.expiry; }
};

struct DnsResolver::State
{
    std::mutex mutex;
    std::chrono::seconds ttl{ std::chrono::seconds(300) };
    std::atomic<uint32_t> writeCount{ 0 };
    std::unordered_map<std::string, CacheEntry> cache;
    std::unordered_map<std::string, std::vector<Promise<Addresses>>> pending;
    std::priority_queue<ExpiryEntry, std::vector<ExpiryEntry>, std::greater<>> expiryQueue;

    static std::string cacheKey(const std::string & hostname, const std::string & service, int family)
    {
        return hostname + "|" + service + "|" + std::to_string(family);
    }

    static Task<DnsResolver::AddressVector> resolveImpl(std::weak_ptr<State> weakState,
                                                        std::shared_ptr<TaskQueue> taskQueue,
                                                        std::string hostname,
                                                        std::string service,
                                                        int family);
    static void doResolve(const std::weak_ptr<DnsResolver::State> & weakState,
                          const std::string & key,
                          const std::string & hostname,
                          const std::string & service,
                          int family);
};

DnsResolver::DnsResolver(std::chrono::seconds ttl, const TaskQueueProvider & taskQueueProvider)
    : taskQueue_(taskQueueProvider()), state_(std::make_shared<State>())
{
    state_->ttl = ttl;
}

DnsResolver::~DnsResolver() = default;

Task<DnsResolver::AddressVector> DnsResolver::resolve(std::string hostname, std::string service)
{
    co_return co_await State::resolveImpl(state_, taskQueue_, std::move(hostname), std::move(service), AF_UNSPEC);
}

Task<DnsResolver::AddressVector> DnsResolver::resolve(std::string hostname, int family)
{
    co_return co_await State::resolveImpl(state_, taskQueue_, std::move(hostname), {}, family);
}

void DnsResolver::State::doResolve(const std::weak_ptr<DnsResolver::State> & weakState,
                                   const std::string & key,
                                   const std::string & hostname,
                                   const std::string & service,
                                   int family)
{
    std::exception_ptr ex;
    std::vector<Promise<Addresses>> waiters;

    struct addrinfo hints = {};
    hints.ai_family = family;
    hints.ai_socktype = SOCK_STREAM;

    struct addrinfo * res = nullptr;
    int error = getaddrinfo(hostname.c_str(),
                            service.empty() ? nullptr : service.c_str(),
                            &hints,
                            &res);

    auto state = weakState.lock();
    if (!state)
    {
        return;
    }

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
        auto expiry = now + state->ttl;
        {
            std::lock_guard lock(state->mutex);
            state->cache[key] = { addresses, expiry };
            state->expiryQueue.push({ expiry, key });
            auto pendingIt = state->pending.find(key);
            if (pendingIt != state->pending.end())
            {
                waiters = std::move(pendingIt->second);
                state->pending.erase(pendingIt);
            }
        }
        // expire old entries every 16 writes to avoid doing it on every write
        if ((state->writeCount.fetch_add(1, std::memory_order_relaxed) & 15) == 0)
        {
            std::lock_guard lock(state->mutex);
            while (!state->expiryQueue.empty() && state->expiryQueue.top().expiry <= now)
            {
                auto & top = state->expiryQueue.top();
                auto cacheIt = state->cache.find(top.key);
                if (cacheIt != state->cache.end() && cacheIt->second.expiry <= now)
                    state->cache.erase(cacheIt);
                state->expiryQueue.pop();
            }
        }
        for (auto & p : waiters)
            p.set_value(addresses);

        return;
    } while (0);

    if (ex)
    {
        {
            std::lock_guard lock(state->mutex);
            auto pendingIt = state->pending.find(key);
            if (pendingIt != state->pending.end())
            {
                waiters = std::move(pendingIt->second);
                state->pending.erase(pendingIt);
            }
        }
        for (auto & p : waiters)
            p.set_exception(ex);
    }
}

Task<DnsResolver::AddressVector> DnsResolver::State::resolveImpl(std::weak_ptr<State> weakState,
                                                                 std::shared_ptr<TaskQueue> taskQueue,
                                                                 std::string hostname,
                                                                 std::string service,
                                                                 int family)
{
    auto state = weakState.lock();
    if (!state)
        co_return {};

    const std::string key = cacheKey(hostname, service, family);

    Promise<Addresses> promise;
    auto future = promise.get_future();
    bool newTask = false;
    auto now = std::chrono::steady_clock::now();

    {
        std::lock_guard lock(state->mutex);

        auto cacheIt = state->cache.find(key);
        if (cacheIt != state->cache.end() && now < cacheIt->second.expiry)
        {
            promise.set_value(cacheIt->second.addresses);
        }
        else if (auto pendingIt = state->pending.find(key); pendingIt != state->pending.end())
        {
            pendingIt->second.push_back(std::move(promise));
        }
        else
        {
            state->pending[key].push_back(std::move(promise));
            newTask = true;
        }
    }

    if (newTask)
    {
        taskQueue->post([weakState, key, hostname, service, family] {
            doResolve(weakState, key, hostname, service, family);
        });
    }

    co_return co_await future.get();
}

} // namespace nitrocoro::net
