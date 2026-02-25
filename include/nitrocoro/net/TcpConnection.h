/**
 * @file TcpConnection.h
 * @brief RAII wrapper for TCP connection file descriptor
 */
#pragma once

#include <netinet/in.h>
#include <nitrocoro/core/Mutex.h>
#include <nitrocoro/core/Task.h>
#include <nitrocoro/io/IoChannel.h>

namespace nitrocoro::net
{

using nitrocoro::Mutex;
using nitrocoro::Task;
using nitrocoro::io::IoChannel;
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

    explicit TcpConnection(std::unique_ptr<IoChannel>);
    ~TcpConnection();

    TcpConnection(const TcpConnection &) = delete;
    TcpConnection & operator=(const TcpConnection &) = delete;
    TcpConnection(TcpConnection &&) = delete;
    TcpConnection & operator=(TcpConnection &&) = delete;

    Task<size_t> read(void * buf, size_t len);
    Task<> write(const void * buf, size_t len);

    Task<> close();
    bool isOpen() const { return fd_ >= 0; }

private:
    int fd_;
    std::unique_ptr<IoChannel> ioChannelPtr_;
    Mutex writeMutex_;
};

} // namespace nitrocoro::net
