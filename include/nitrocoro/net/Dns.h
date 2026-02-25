/**
 * @file Dns.h
 * @brief Global DNS resolution functions
 */
#pragma once

#include <nitrocoro/core/Task.h>
#include <nitrocoro/net/InetAddress.h>
#include <string>
#include <vector>

namespace nitrocoro::net
{

Task<std::vector<InetAddress>> resolve(const std::string & hostname);

} // namespace nitrocoro::net
