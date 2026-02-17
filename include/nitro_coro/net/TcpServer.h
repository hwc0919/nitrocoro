/**
 * @file TcpServer.h
 * @brief Coroutine-based TCP server component
 */
#pragma once

#include <nitro_coro/core/Future.h>
#include <nitro_coro/core/Scheduler.h>
#include <nitro_coro/core/Task.h>
#include <nitro_coro/net/TcpConnection.h>

#include <atomic>
#include <functional>
#include <memory>
#include <unordered_set>

namespace nitro_coro::net
{

using nitro_coro::Scheduler;
using nitro_coro::Task;
using nitro_coro::io::IoChannel;

class TcpServer
{
public:
    using ConnectionHandler = std::function<Task<>(std::shared_ptr<TcpConnection>)>;

    explicit TcpServer(Scheduler * scheduler, int port);
    ~TcpServer();

    Task<> start(ConnectionHandler handler);
    Task<> stop();

private:
    void setup_socket();

    Scheduler * scheduler_;
    int listenFd_;
    int port_;
    std::atomic_bool started_{ false };
    std::atomic_bool stopped_{ false };
    Promise<> stopPromise_;
    std::shared_ptr<IoChannel> listenChannel_;

    using ConnectionSet = std::unordered_set<TcpConnectionPtr>;
    std::shared_ptr<ConnectionSet> connSetPtr_{ std::make_shared<ConnectionSet>() };
};

} // namespace nitro_coro::net
