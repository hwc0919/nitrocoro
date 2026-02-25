/**
 * @file Dns.cc
 * @brief Global DNS resolution implementation
 */
#include <nitrocoro/net/Dns.h>
#include <nitrocoro/net/DnsResolver.h>

namespace nitrocoro::net
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

} // namespace nitrocoro::net
