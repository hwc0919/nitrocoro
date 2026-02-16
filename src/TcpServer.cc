/**
 * @file TcpServer.cc
 * @brief Implementation of coroutine-based TCP server
 */
#include <nitro_coro/core/Scheduler.h>
#include <nitro_coro/net/TcpConnection.h>
#include <nitro_coro/net/TcpServer.h>

#include <cstring>
#include <fcntl.h>
#include <iostream>
#include <netinet/in.h>
#include <stdexcept>
#include <sys/socket.h>
#include <unistd.h>

namespace nitro_coro::net
{
using nitro_coro::Scheduler;
using nitro_coro::Task;
using nitro_coro::io::IoChannel;
using nitro_coro::io::TriggerMode;

TcpServer::TcpServer(Scheduler * scheduler, int port) : scheduler_(scheduler), listen_fd_(-1), port_(port), running_(false)
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
        throw std::runtime_error(std::string("Failed to bind socket: ") + strerror(errno));
    }
}

struct Acceptor
{
    IoChannel::IoResult read(int fd, IoChannel *)
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

Task<> TcpServer::start(ConnectionHandler handler)
{
    co_await scheduler_->run_here();

    if (listen(listen_fd_, 128) < 0)
    {
        close(listen_fd_);
        throw std::runtime_error(std::string("Failed to listen on socket: ") + strerror(errno));
    }
    std::cout << "TcpServer listening on port " << port_ << "\n";

    listenChannel_ = IoChannel::create(listen_fd_, scheduler_, TriggerMode::LevelTriggered);
    listenChannel_->enableReading();
    running_ = true;
    [[maybe_unused]] auto lock = co_await closeMutex_.scoped_lock();
    while (running_.load())
    {
        Acceptor acceptor;
        try
        {
            co_await listenChannel_->performRead(&acceptor);
        }
        catch (const std::exception & e)
        {
            std::cout << "Accept error: " << e.what() << "\n";
            break;
        }

        std::cout << "Accepted connection: fd=" << acceptor.clientFd() << "\n";
        auto ioChannelPtr = IoChannel::create(acceptor.clientFd(), scheduler_, TriggerMode::EdgeTriggered);
        ioChannelPtr->enableReading();
        auto connPtr = std::make_shared<TcpConnection>(std::move(ioChannelPtr));
        scheduler_->spawn([handler, connPtr = std::move(connPtr)]() mutable -> Task<> {
            try
            {
                co_await handler(std::move(connPtr));
            }
            catch (...)
            {
                printf("Exception escaped from TcpServer handler");
            }
        });
    }
    listenChannel_->disableAll();
    if (listen_fd_ >= 0)
    {
        ::close(listen_fd_);
        listen_fd_ = -1;
    }
    printf("TcpServer::start() quit\n");
}

Task<> TcpServer::stop()
{
    if (!running_.exchange(false))
        co_return;

    co_await scheduler_->run_here();
    printf("TcpServer::stop() requested\n");
    listenChannel_->cancelAll();
    [[maybe_unused]] auto lock = co_await closeMutex_.scoped_lock();
}

} // namespace nitro_coro::net
