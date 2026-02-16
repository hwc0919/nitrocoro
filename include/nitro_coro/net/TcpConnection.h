/**
 * @file TcpConnection.h
 * @brief RAII wrapper for TCP connection file descriptor
 */
#pragma once

#include <nitro_coro/io/IoChannel.h>
#include <nitro_coro/sync/Mutex.h>
#include <nitro_coro/core/Task.h>
#include <netinet/in.h>

namespace nitro_coro
{
class TcpConnection;
using TcpConnectionPtr = std::shared_ptr<TcpConnection>;

class TcpConnection
{
public:
    enum class IpVersion
    {
        Ipv4,
        Ipv6
    };

    static Task<TcpConnectionPtr> connect(const sockaddr * addr, socklen_t addrLen);
    static Task<TcpConnectionPtr> connect(const char * ip, uint16_t port, IpVersion v = IpVersion::Ipv4);

    explicit TcpConnection(std::shared_ptr<IoChannel>);
    ~TcpConnection();

    TcpConnection(const TcpConnection &) = delete;
    TcpConnection & operator=(const TcpConnection &) = delete;
    TcpConnection(TcpConnection &&) = delete;
    TcpConnection & operator=(TcpConnection &&) = delete;

    Task<size_t> read(void * buf, size_t len);
    Task<> write(const void * buf, size_t len);

    Task<> close();
    Task<> finishWriteAndClose();
    bool is_open() const { return fd_ >= 0; }

private:
    int fd_;
    std::shared_ptr<IoChannel> ioChannelPtr_;
    Mutex writeMutex_;
};

} // namespace nitro_coro
