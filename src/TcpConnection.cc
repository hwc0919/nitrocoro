/**
 * @file TcpConnection.cc
 * @brief Implementation of TcpConnection
 */
#include "TcpConnection.h"
#include "Scheduler.h"
#include <arpa/inet.h>
#include <fcntl.h>
#include <iostream>
#include <unistd.h>

namespace my_coro
{

struct Connector
{
    Connector(const sockaddr * addr, socklen_t addrLen)
        : addr_(addr), addrLen_(addrLen)
    {
    }

    IoChannel::IoResult write(int fd, IoChannel * channel)
    {
        if (connecting_)
        {
            int error = 0;
            socklen_t len = sizeof(error);
            if (getsockopt(fd, SOL_SOCKET, SO_ERROR, &error, &len) < 0)
            {
                return IoChannel::IoResult::Error;
            }
            if (error == 0)
            {
                channel->disableWriting();
                return IoChannel::IoResult::Success;
            }
            else if (error == EINPROGRESS || error == EALREADY)
            {
                return IoChannel::IoResult::WouldBlock;
            }
            else
            {
                return IoChannel::IoResult::Error;
            }
        }

        int ret = ::connect(fd, addr_, addrLen_);
        if (ret == 0)
        {
            channel->disableWriting();
            return IoChannel::IoResult::Success;
        }
        int lastErrno = errno;
        switch (lastErrno)
        {
            case EISCONN:
                channel->disableWriting();
                return IoChannel::IoResult::Success;
            case EINPROGRESS:
            case EALREADY:
                connecting_ = true;
                channel->enableWriting();
                return IoChannel::IoResult::WouldBlock;
            case EINTR:
                return IoChannel::IoResult::Retry;

            default:
                return IoChannel::IoResult::Error;
        }
    }

private:
    const sockaddr * addr_;
    socklen_t addrLen_;

    bool connecting_{ false };
};

Task<TcpConnectionPtr> TcpConnection::connect(const sockaddr * addr, socklen_t addrLen)
{
    int fd = socket(addr->sa_family, SOCK_STREAM, 0);
    if (fd < 0)
    {
        throw std::runtime_error("Failed to create socket");
    }

    int flags = fcntl(fd, F_GETFL, 0);
    fcntl(fd, F_SETFL, flags | O_NONBLOCK);

    auto channelPtr = std::make_unique<IoChannel>(fd, Scheduler::current());
    Connector connector(addr, addrLen);
    co_await channelPtr->performWrite(&connector);

    co_return std::make_shared<TcpConnection>(std::move(channelPtr));
}

Task<TcpConnectionPtr> TcpConnection::connect(const char * ip, uint16_t port, TcpConnection::IpVersion v)
{
    if (v == IpVersion::Ipv4)
    {
        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_port = htons(port);
        if (inet_pton(AF_INET, ip, &addr.sin_addr) != 1)
        {
            throw std::runtime_error("Invalid IPv4 address");
        }
        co_return co_await connect(reinterpret_cast<sockaddr *>(&addr), static_cast<socklen_t>(sizeof(addr)));
    }
    else
    {
        sockaddr_in6 addr{};
        addr.sin6_family = AF_INET6;
        addr.sin6_port = htons(port);
        if (inet_pton(AF_INET6, ip, &addr.sin6_addr) != 1)
        {
            throw std::runtime_error("Invalid IPv6 address");
        }
        co_return co_await connect(reinterpret_cast<sockaddr *>(&addr), static_cast<socklen_t>(sizeof(addr)));
    }
}

TcpConnection::TcpConnection(std::unique_ptr<IoChannel> channelPtr)
    : fd_(channelPtr->fd())
    , ioChannelPtr_(std::move(channelPtr))
{
    ioChannelPtr_->enableReading();
}

TcpConnection::~TcpConnection() = default;

Task<size_t> TcpConnection::read(void * buf, size_t len)
{
    BufferReader reader(buf, len);
    co_await ioChannelPtr_->performRead(&reader);
    co_return reader.readLen();
}

Task<> TcpConnection::write(const void * buf, size_t len)
{
    [[maybe_unused]] auto lock = co_await writeMutex_.scoped_lock();
    BufferWriter writer(buf, len);
    co_await ioChannelPtr_->performWrite(&writer);
}

Task<> TcpConnection::close()
{
    // TODO
    co_return;
}

Task<> TcpConnection::finishWriteAndClose()
{
    // TODO
    co_return;
}

} // namespace my_coro
