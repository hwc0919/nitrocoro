/**
 * @file TcpServer.h
 * @brief Coroutine-based TCP server component
 */
#pragma once

#include "CoroScheduler.h"
#include "TcpConnection.h"
#include <functional>
#include <memory>

namespace my_coro
{

class TcpServer
{
public:
    using ConnectionHandler = std::function<Task(std::shared_ptr<TcpConnection>)>;

    explicit TcpServer(int port);
    ~TcpServer();

    void set_handler(ConnectionHandler handler);
    Task start();
    void stop();

private:
    int listen_fd_;
    int port_;
    bool running_;
    ConnectionHandler handler_;

    void setup_socket();
};

} // namespace my_coro
