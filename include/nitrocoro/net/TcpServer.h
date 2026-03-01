/**
 * @file TcpServer.h
 * @brief Coroutine-based TCP server component
 */
#pragma once

#include <nitrocoro/core/Future.h>
#include <nitrocoro/core/Scheduler.h>
#include <nitrocoro/core/Task.h>
#include <nitrocoro/net/Socket.h>
#include <nitrocoro/net/TcpConnection.h>

#include <atomic>
#include <functional>
#include <memory>
#include <unordered_set>

namespace nitrocoro::net
{

using nitrocoro::Scheduler;
using nitrocoro::Task;
using nitrocoro::io::IoChannel;

class TcpServer
{
public:
    using ConnectionHandler = std::function<Task<>(std::shared_ptr<TcpConnection>)>;

    explicit TcpServer(uint16_t port, Scheduler * scheduler = Scheduler::current());
    ~TcpServer();

    Task<> start(ConnectionHandler handler);
    Task<> stop();
    Task<> wait() const;

    uint16_t port() const { return port_; }

private:
    void setup_socket();

    uint16_t port_;
    Scheduler * scheduler_;
    std::shared_ptr<net::Socket> listenSocketPtr_;
    std::atomic_bool started_{ false };
    std::atomic_bool stopped_{ false };
    Promise<> stopPromise_;
    SharedFuture<> stopFuture_;
    std::unique_ptr<IoChannel> listenChannel_;

    using ConnectionSet = std::unordered_set<TcpConnectionPtr>;
    std::shared_ptr<ConnectionSet> connSetPtr_{ std::make_shared<ConnectionSet>() };
};

} // namespace nitrocoro::net
