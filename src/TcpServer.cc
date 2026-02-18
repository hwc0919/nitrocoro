/**
 * @file TcpServer.cc
 * @brief Implementation of coroutine-based TCP server
 */
#include <nitro_coro/core/Scheduler.h>
#include <nitro_coro/net/TcpConnection.h>
#include <nitro_coro/net/TcpServer.h>
#include <nitro_coro/utils/Debug.h>

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

TcpServer::TcpServer(uint16_t port, Scheduler * scheduler)
    : port_(port)
    , scheduler_(scheduler)
    , stopPromise_(scheduler)
    , stopFuture_(stopPromise_.get_future().share())
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
        listenFd_ = -1;
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
    co_await scheduler_->switch_to();
    if (started_.exchange(true))
    {
        throw std::logic_error("TcpServer already started");
    }

    if (listen(listenFd_, 128) < 0)
    {
        stopped_.store(true);
        stopPromise_.set_value();
        throw std::runtime_error(std::string("Failed to listen on socket: ") + strerror(errno));
    }
    NITRO_INFO("TcpServer listening on port %hu\n", port_);

    auto handlerPtr = std::make_shared<ConnectionHandler>(std::move(handler));
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
            if (acceptor.clientFd() >= 0)
                ::close(acceptor.clientFd());
            NITRO_ERROR("Accept error: %s\n", e.what());
            break;
        }

        NITRO_DEBUG("Accepted connection: fd = %d\n", acceptor.clientFd());
        auto ioChannelPtr = IoChannel::create(acceptor.clientFd(), scheduler_, TriggerMode::EdgeTriggered);
        ioChannelPtr->enableReading();
        auto connPtr = std::make_shared<TcpConnection>(std::move(ioChannelPtr));
        connSetPtr_->insert(connPtr);
        std::weak_ptr<ConnectionSet> weakConnSet{ connSetPtr_ };
        scheduler_->spawn([scheduler = scheduler_, handlerPtr, connPtr, weakConnSet]() mutable -> Task<> {
            try
            {
                co_await (*handlerPtr)(connPtr);
            }
            catch (...)
            {
                NITRO_ERROR("Exception escaped from TcpServer handler\n");
            }
            co_await scheduler->switch_to();
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
    NITRO_INFO("TcpServer::start() quit\n");
}

Task<> TcpServer::stop()
{
    co_await scheduler_->switch_to();
    if (stopped_.exchange(true))
        co_return;

    listenChannel_->disableAll(); // stop listening first
    listenChannel_->cancelAll();

    std::vector<TcpConnectionPtr> conns(connSetPtr_->begin(), connSetPtr_->end());
    for (auto & c : conns)
    {
        co_await c->close();
    }
    NITRO_INFO("TcpServer::stop() requested\n");
    co_await stopFuture_.get();
}

Task<> TcpServer::wait() const
{
    co_await stopFuture_.get();
}

} // namespace nitro_coro::net
