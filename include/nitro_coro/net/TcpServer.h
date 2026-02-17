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

    explicit TcpServer(uint16_t port, Scheduler * scheduler = Scheduler::current());
    ~TcpServer();

    Task<> start(ConnectionHandler handler);
    Task<> stop();
    Task<> wait() const;

private:
    void setup_socket();

    uint16_t port_;
    Scheduler * scheduler_;
    int listenFd_{ -1 };
    std::atomic_bool started_{ false };
    std::atomic_bool stopped_{ false };
    Promise<> stopPromise_;
    SharedFuture<> stopFuture_;
    std::shared_ptr<IoChannel> listenChannel_;

    using ConnectionSet = std::unordered_set<TcpConnectionPtr>;
    std::shared_ptr<ConnectionSet> connSetPtr_{ std::make_shared<ConnectionSet>() };
};

} // namespace nitro_coro::net
