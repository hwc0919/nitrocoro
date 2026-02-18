/**
 * @file Dns.h
 * @brief Global DNS resolution functions
 */
#pragma once

#include <nitro_coro/core/Task.h>
#include <nitro_coro/net/InetAddress.h>
#include <string>
#include <vector>

namespace nitro_coro::net
{

Task<std::vector<InetAddress>> resolve(const std::string & hostname);

} // namespace nitro_coro::net
