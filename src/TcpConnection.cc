/**
 * @file TcpConnection.cc
 * @brief Implementation of TcpConnection
 */
#include <arpa/inet.h>
#include <nitrocoro/core/Scheduler.h>
#include <nitrocoro/io/Socket.h>
#include <nitrocoro/io/adapters/BufferReader.h>
#include <nitrocoro/io/adapters/BufferWriter.h>
#include <nitrocoro/net/TcpConnection.h>

namespace nitrocoro::net
{

using nitrocoro::Scheduler;
using nitrocoro::Task;
using nitrocoro::io::IoChannel;
using nitrocoro::io::Socket;
using nitrocoro::io::adapters::BufferReader;
using nitrocoro::io::adapters::BufferWriter;

struct Connector
{
    Connector(const sockaddr * addr, socklen_t addrLen)
        : addr_(addr)
        , addrLen_(addrLen)
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
    int fd = ::socket(addr->sa_family, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0);
    if (fd < 0)
        throw std::runtime_error("Failed to create socket");
    auto socket = std::make_shared<Socket>(fd);
    auto channelPtr = std::make_unique<IoChannel>(fd);
    channelPtr->setGuard(socket);
    Connector connector(addr, addrLen);
    auto result = co_await channelPtr->performWrite(&connector);
    if (result != IoChannel::IoResult::Success)
        throw std::runtime_error("TCP connect failed");

    co_return std::make_shared<TcpConnection>(std::move(channelPtr), std::move(socket));
}

Task<TcpConnectionPtr> TcpConnection::connect(const char * ip, uint16_t port, IpVersion v)
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
        co_return co_await connect(reinterpret_cast<sockaddr *>(&addr), sizeof(addr));
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
        co_return co_await connect(reinterpret_cast<sockaddr *>(&addr), sizeof(addr));
    }
}

TcpConnection::TcpConnection(std::unique_ptr<IoChannel> channelPtr, std::shared_ptr<Socket> socket)
    : socket_(std::move(socket))
    , ioChannelPtr_(std::move(channelPtr))
{
    ioChannelPtr_->enableReading();
}

TcpConnection::~TcpConnection() = default;

Task<size_t> TcpConnection::read(void * buf, size_t len)
{
    BufferReader reader(buf, len);
    auto result = co_await ioChannelPtr_->performRead(&reader);
    if (result == IoChannel::IoResult::Eof)
        co_return 0;
    if (result != IoChannel::IoResult::Success)
        throw std::runtime_error("TCP read error");
    co_return reader.readLen();
}

Task<size_t> TcpConnection::write(const void * buf, size_t len)
{
    [[maybe_unused]] auto lock = co_await writeMutex_.scoped_lock();
    BufferWriter writer(buf, len);
    auto result = co_await ioChannelPtr_->performWrite(&writer);
    if (result == IoChannel::IoResult::Eof)
        co_return 0;
    if (result != IoChannel::IoResult::Success)
        throw std::runtime_error("TCP write error");
    co_return len;
}

Task<> TcpConnection::shutdown()
{
    // TODO: need status flag
    co_await ioChannelPtr_->scheduler()->switch_to();
    socket_->shutdownWrite();
}

Task<> TcpConnection::forceClose()
{
    // TODO: need status flag
    co_await ioChannelPtr_->scheduler()->switch_to();
    ioChannelPtr_->disableAll();
    ioChannelPtr_->cancelAll();
}

} // namespace nitrocoro::net
