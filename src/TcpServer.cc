/**
 * @file TcpServer.cc
 * @brief Implementation of coroutine-based TCP server
 */
#include "TcpServer.h"
#include "Scheduler.h"
#include "TcpConnection.h"

#include <cstring>
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

    listenChannel_ = std::make_unique<IoChannel>(listen_fd_, Scheduler::current(), TriggerMode::LevelTriggered);
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


struct Acceptor : public IoChannel::IoReader
{
    IoChannel::IoResult read(int fd) override
    {
        socklen_t len = sizeof(clientAddr_);
        fd_ = ::accept4(fd, reinterpret_cast<struct sockaddr *>(&clientAddr_), &len, SOCK_NONBLOCK | SOCK_CLOEXEC);
        if (fd_ >= 0)
        {
            return IoChannel::IoResult::Success;
        }
        else
        {
            switch (errno)
            {
                case EAGAIN:
#if EAGAIN != EWOULDBLOCK
                case EWOULDBLOCK:
#endif
                    return IoChannel::IoResult::WouldBlock;
                case EINTR:
                    return IoChannel::IoResult::Retry;
                default:
                    return IoChannel::IoResult::Error;
            }
        }
    }
    int clientFd() const { return fd_; }
    const struct sockaddr_in & clientAddr() const { return clientAddr_; }

private:
    struct sockaddr_in clientAddr_{};
    int fd_{ -1 };
};

Task<> TcpServer::start()
{
    if (!handler_)
    {
        throw std::runtime_error("No connection handler set");
    }

    running_ = true;
    while (running_)
    {
        Acceptor acceptor;
        co_await listenChannel_->performRead(&acceptor);

        std::cout << "Accepted connection: fd=" << acceptor.clientFd() << "\n";

        auto conn = std::make_shared<TcpConnection>(acceptor.clientFd());
        Scheduler::current()->spawn([this, conn]() -> Task<> {
            co_await handler_(conn);
        });
    }
}

void TcpServer::stop()
{
    running_ = false;
}

} // namespace my_coro
