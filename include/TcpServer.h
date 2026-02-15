/**
 * @file TcpServer.h
 * @brief Coroutine-based TCP server component
 */
#pragma once

#include "Task.h"
#include "TcpConnection.h"
#include <functional>
#include <memory>

namespace my_coro
{

class TcpServer
{
public:
    using ConnectionHandler = std::function<Task<>(std::shared_ptr<TcpConnection>)>;

    explicit TcpServer(Scheduler * scheduler, int port);
    ~TcpServer();

    Task<> start(ConnectionHandler handler);
    void stop();

private:
    Scheduler * scheduler_;
    int listen_fd_;
    int port_;
    bool running_; // TODO

    std::shared_ptr<IoChannel> listenChannel_;

    void setup_socket();
};

} // namespace my_coro
