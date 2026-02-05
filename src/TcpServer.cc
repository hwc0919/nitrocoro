/**
 * @file TcpServer.cc
 * @brief Implementation of coroutine-based TCP server
 */
#include "TcpServer.h"
#include "CoroScheduler.h"
#include "TcpConnection.h"

#include <fcntl.h>
#include <iostream>
#include <netinet/in.h>
#include <stdexcept>
#include <sys/socket.h>
#include <unistd.h>

namespace my_coro
{

TcpServer::TcpServer(int port) : listen_fd_(-1), port_(port), running_(false)
{
    setup_socket();
}

TcpServer::~TcpServer()
{
    if (listen_fd_ >= 0)
    {
        close(listen_fd_);
    }
}

void TcpServer::setup_socket()
{
    listen_fd_ = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_fd_ < 0)
    {
        throw std::runtime_error("Failed to create socket");
    }

    int opt = 1;
    setsockopt(listen_fd_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    int flags = fcntl(listen_fd_, F_GETFL, 0);
    fcntl(listen_fd_, F_SETFL, flags | O_NONBLOCK);

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port_);

    if (bind(listen_fd_, (sockaddr *)&addr, sizeof(addr)) < 0)
    {
        close(listen_fd_);
        throw std::runtime_error("Failed to bind socket");
    }

    if (listen(listen_fd_, 128) < 0)
    {
        close(listen_fd_);
        throw std::runtime_error("Failed to listen on socket");
    }

    std::cout << "TcpServer listening on port " << port_ << "\n";
}

void TcpServer::set_handler(ConnectionHandler handler)
{
    handler_ = std::move(handler);
}

Task<> TcpServer::start()
{
    if (!handler_)
    {
        throw std::runtime_error("No connection handler set");
    }

    running_ = true;
    char dummy;

    while (running_)
    {
        co_await CoroScheduler::current()->async_read(listen_fd_, &dummy, 1);

        sockaddr_in client_addr{};
        socklen_t addr_len = sizeof(client_addr);
        int client_fd = accept(listen_fd_, (sockaddr *)&client_addr, &addr_len);

        if (client_fd < 0)
        {
            continue;
        }

        int flags = fcntl(client_fd, F_GETFL, 0);
        fcntl(client_fd, F_SETFL, flags | O_NONBLOCK);

        std::cout << "Accepted connection: fd=" << client_fd << "\n";

        auto conn = std::make_shared<TcpConnection>(client_fd);
        CoroScheduler::current()->spawn([this, conn]() -> Task<> {
            co_await handler_(conn);
        });
    }
}

void TcpServer::stop()
{
    running_ = false;
}

} // namespace my_coro
