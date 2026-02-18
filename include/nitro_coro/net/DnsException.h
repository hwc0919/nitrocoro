/**
 * @file DnsException.h
 * @brief DNS resolution exception
 */
#pragma once

#include <exception>
#include <string>

namespace nitro_coro::net
{

class DnsException : public std::exception
{
public:
    DnsException(const char * message, int error_code)
        : message_(message), error_code_(error_code)
    {
    }

    const char * what() const noexcept override { return message_.c_str(); }
    int errorCode() const noexcept { return error_code_; }

private:
    std::string message_;
    int error_code_;
};

} // namespace nitro_coro::net
