/**
 * @file TcpServer.h
 * @brief Coroutine-based TCP server component
 */
#pragma once

#include "CoroScheduler.h"
#include <functional>

namespace my_coro
{

class TcpServer
{
public:
    using ConnectionHandler = std::function<Task(int fd)>;

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
