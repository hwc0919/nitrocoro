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

TcpServer::TcpServer(Scheduler * scheduler, int port)
    : scheduler_(scheduler)
    , listenFd_(-1)
    , port_(port)
    , stopPromise_(scheduler)
{
    setup_socket();
}

TcpServer::~TcpServer()
{
    if (listenFd_ >= 0)
    {
        close(listenFd_);
    }
}

void TcpServer::setup_socket()
{
    listenFd_ = socket(AF_INET, SOCK_STREAM, 0);
    if (listenFd_ < 0)
    {
        throw std::runtime_error("Failed to create socket");
    }

    int opt = 1;
    setsockopt(listenFd_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    int flags = fcntl(listenFd_, F_GETFL, 0);
    fcntl(listenFd_, F_SETFL, flags | O_NONBLOCK);

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port_);

    if (bind(listenFd_, (sockaddr *)&addr, sizeof(addr)) < 0)
    {
        close(listenFd_);
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
    if (started_.exchange(true))
    {
        throw std::runtime_error("TcpServer already started");
    }

    co_await scheduler_->run_here();

    if (listen(listenFd_, 128) < 0)
    {
        close(listenFd_);
        throw std::runtime_error(std::string("Failed to listen on socket: ") + strerror(errno));
    }
    std::cout << "TcpServer listening on port " << port_ << "\n";

    listenChannel_ = IoChannel::create(listenFd_, scheduler_, TriggerMode::LevelTriggered);
    listenChannel_->enableReading();
    while (!stopped_.load())
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
        connSetPtr_->insert(connPtr);
        std::weak_ptr<ConnectionSet> weakConnSet{ connSetPtr_ };
        scheduler_->spawn([scheduler = scheduler_, handler, connPtr, weakConnSet]() mutable -> Task<> {
            try
            {
                co_await handler(connPtr);
            }
            catch (...)
            {
                printf("Exception escaped from TcpServer handler");
            }
            co_await scheduler->run_here();
            if (auto connSetPtr = weakConnSet.lock())
            {
                connSetPtr->erase(connPtr);
            }
            co_await connPtr->close();
        });
    }
    listenChannel_->disableAll();
    if (listenFd_ >= 0)
    {
        ::close(listenFd_);
        listenFd_ = -1;
    }
    stopPromise_.set_value();
    printf("TcpServer::start() quit\n");
}

Task<> TcpServer::stop()
{
    if (stopped_.exchange(true))
        co_return;

    co_await scheduler_->run_here();
    listenChannel_->disableAll(); // stop listening first
    listenChannel_->cancelAll();

    std::vector<TcpConnectionPtr> conns(connSetPtr_->begin(), connSetPtr_->end());
    for (auto & c : conns)
    {
        co_await c->close();
    }
    printf("TcpServer::stop() requested\n");
    co_await stopPromise_.get_future().get();
}

} // namespace nitro_coro::net
