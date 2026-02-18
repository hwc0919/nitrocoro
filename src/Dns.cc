/**
 * @file Dns.cc
 * @brief Global DNS resolution implementation
 */
#include <nitro_coro/net/Dns.h>
#include <nitro_coro/net/DnsResolver.h>

namespace nitro_coro::net
{

static DnsResolver & getGlobalResolver()
{
    static DnsResolver resolver;
    return resolver;
}

Task<std::vector<InetAddress>> resolve(const std::string & hostname)
{
    co_return co_await getGlobalResolver().resolve(hostname);
}

} // namespace nitro_coro::net
