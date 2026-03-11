/**
 * @file dns_test.cc
 * @brief Tests for DnsResolver. Requires network access.
 */
#include <nitrocoro/net/DnsResolver.h>
#include <nitrocoro/testing/Test.h>
#include <nitrocoro/utils/TaskQueue.h>

#include <netdb.h>
#include <set>

using namespace nitrocoro;
using namespace nitrocoro::net;

// Synchronous reference resolver using the same getaddrinfo() that ping uses.
static std::set<std::string> resolveSync(const std::string & host)
{
    std::set<std::string> result;
    addrinfo hints{};
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    addrinfo * res = nullptr;
    if (::getaddrinfo(host.c_str(), nullptr, &hints, &res) != 0 || !res)
        return result;
    for (auto * p = res; p; p = p->ai_next)
    {
        if (p->ai_family == AF_INET)
        {
            InetAddress addr(*reinterpret_cast<sockaddr_in *>(p->ai_addr));
            result.insert(addr.toIp());
        }
        else if (p->ai_family == AF_INET6)
        {
            InetAddress addr(*reinterpret_cast<sockaddr_in6 *>(p->ai_addr));
            result.insert(addr.toIp());
        }
    }
    ::freeaddrinfo(res);
    return result;
}

// ── Helpers ───────────────────────────────────────────────────────────────────

struct CountingTaskQueue : TaskQueue
{
    std::atomic<int> count{ 0 };
    ThreadPool pool{ 2 };
    void post(std::function<void()> fn) override
    {
        ++count;
        pool.post(std::move(fn));
    }
};

// ── Tests ─────────────────────────────────────────────────────────────────────

/** Resolving "localhost" returns at least one loopback address. */
NITRO_TEST(dns_localhost)
{
    DnsResolver resolver;
    auto addrs = co_await resolver.resolve("localhost");
    NITRO_CHECK(!addrs.empty());
    bool hasLoopback = false;
    for (auto & a : addrs)
        if (a.isLoopbackIp())
            hasLoopback = true;
    NITRO_CHECK(hasLoopback);
}

/** Resolving a non-existent domain throws DnsException. */
NITRO_TEST(dns_invalid_domain)
{
    DnsResolver resolver;
    bool threw = false;
    try
    {
        co_await resolver.resolve("this.domain.does.not.exist.invalid");
    }
    catch (const DnsException &)
    {
        threw = true;
    }
    NITRO_CHECK(threw);
}

/**
 * For each well-known globally reachable host, DnsResolver must return at
 * least one IP that also appears in the synchronous getaddrinfo() result
 * (the same source ping uses). CDN domains may return different subsets on
 * each call, so we only require a non-empty intersection.
 */
NITRO_TEST(dns_matches_system_resolver)
{
    static const char * hosts[] = {
        "www.baidu.com",      // CN + global
        "www.cloudflare.com", // global
        "www.microsoft.com",  // global
    };

    DnsResolver resolver;
    for (const char * host : hosts)
    {
        auto ref = resolveSync(host);
        NITRO_CHECK(!ref.empty()); // system resolver must work too

        auto addrs = co_await resolver.resolve(host);
        NITRO_CHECK(!addrs.empty());

        bool intersects = false;
        for (auto & a : addrs)
            if (ref.count(a.toIp()))
                intersects = true;
        NITRO_CHECK(intersects);
    }
}

/** Second resolve of the same host hits the cache — no extra post(). */
NITRO_TEST(dns_cache_hit)
{
    auto queue = std::make_shared<CountingTaskQueue>();
    DnsResolver resolver(std::chrono::seconds(60), [queue] { return queue; });

    for (int i = 0; i < 3; ++i)
    {
        co_await resolver.resolve("localhost");
        NITRO_CHECK_EQ(queue->count.load(), 1);
    }
}

/** Concurrent resolves for the same host trigger only one post(). */
NITRO_TEST(dns_concurrent_dedup)
{
    auto queue = std::make_shared<CountingTaskQueue>();
    DnsResolver resolver(std::chrono::seconds(60), [queue] { return queue; });

    int cnt{ 0 };
    Promise<> allDone;
    auto allDoneFuture = allDone.get_future().share();

    for (int i = 0; i < 3; ++i)
        Scheduler::current()->spawn([&]() -> Task<> {
            co_await resolver.resolve("localhost");
            if (++cnt == 3)
                allDone.set_value();
        });

    co_await allDoneFuture;
    NITRO_CHECK_EQ(queue->count.load(), 1);
}

int main(int argc, char ** argv)
{
    return nitrocoro::test::run_all(argc, argv);
}
