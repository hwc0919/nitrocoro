/**
 * @file dns_resolver_test.cc
 * @brief DNS resolver test
 */
#include <nitrocoro/core/Scheduler.h>
#include <nitrocoro/net/DnsException.h>
#include <nitrocoro/net/DnsResolver.h>
#include <nitrocoro/utils/Debug.h>

using namespace nitrocoro;
using namespace nitrocoro::net;

Task<> test_dns()
{
    DnsResolver resolver;

    const char * hosts[] = { "www.baidu.com", "www.github.com", "localhost", "invalid.domain.test" };

    for (const char * host : hosts)
    {
        NITRO_INFO("Resolving %s...\n", host);
        try
        {
            auto addresses = co_await resolver.resolve(host);
            NITRO_INFO("  Found %zu address(es):\n", addresses.size());
            for (const auto & addr : addresses)
            {
                NITRO_INFO("    %s\n", addr.toIpPort().c_str());
            }
        }
        catch (const DnsException & e)
        {
            NITRO_ERROR("  Failed: %s\n", e.what());
        }
    }
}

int main()
{
    NITRO_INFO("=== DNS Resolver Test ===\n");

    Scheduler scheduler;
    scheduler.spawn([&scheduler]() -> Task<> {
        co_await test_dns();
        scheduler.stop();
    });
    scheduler.run();
    return 0;
}
