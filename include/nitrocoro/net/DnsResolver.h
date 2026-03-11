/**
 * @file DnsResolver.h
 * @brief Asynchronous DNS resolver using thread pool
 */
#pragma once

#include <nitrocoro/core/Task.h>
#include <nitrocoro/net/InetAddress.h>
#include <nitrocoro/utils/TaskQueue.h>

#include <chrono>
#include <memory>
#include <string>

namespace nitrocoro::net
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

class DnsResolver
{
public:
    struct State;
    using AddressVector = std::vector<InetAddress>;

    explicit DnsResolver(std::chrono::seconds ttl = std::chrono::seconds(300),
                         const TaskQueueProvider & taskQueueProvider = defaultTaskQueueProvider());
    ~DnsResolver();

    DnsResolver(const DnsResolver &) = delete;
    DnsResolver & operator=(const DnsResolver &) = delete;

    Task<AddressVector> resolve(std::string hostname, std::string service = "");
    Task<AddressVector> resolve(std::string hostname, int family);

private:
    std::shared_ptr<TaskQueue> taskQueue_;
    std::shared_ptr<State> state_;
};

} // namespace nitrocoro::net
