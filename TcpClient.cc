/**
 * @file TcpClient.cc
 * @brief Implementation of coroutine-based TCP client
 */
#include "TcpClient.h"
#include "CoroScheduler.h"
#include <arpa/inet.h>
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <netinet/in.h>
#include <stdexcept>
#include <sys/socket.h>
#include <unistd.h>

namespace my_coro
{

TcpClient::TcpClient() : fd_(-1)
{
}

TcpClient::~TcpClient()
{
    close();
}

Task<> TcpClient::connect(const char * host, int port)
{
    fd_ = socket(AF_INET, SOCK_STREAM, 0);
    if (fd_ < 0)
    {
        throw std::runtime_error("Failed to create socket");
    }

    int flags = fcntl(fd_, F_GETFL, 0);
    fcntl(fd_, F_SETFL, flags | O_NONBLOCK);

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    inet_pton(AF_INET, host, &addr.sin_addr);

    int ret = ::connect(fd_, (sockaddr *)&addr, sizeof(addr));

    if (ret < 0 && errno != EINPROGRESS)
    {
        ::close(fd_);
        fd_ = -1;
        throw std::runtime_error("Connect failed");
    }

    // Wait for connection to complete (fd becomes writable)
    char dummy;
    co_await current_scheduler()->async_write(fd_, &dummy, 0);

    // Check if connection succeeded
    int error = 0;
    socklen_t len = sizeof(error);
    getsockopt(fd_, SOL_SOCKET, SO_ERROR, &error, &len);

    if (error != 0)
    {
        ::close(fd_);
        fd_ = -1;
        throw std::runtime_error("Connect failed: " + std::string(strerror(error)));
    }
}

Task<ssize_t> TcpClient::read(void * buf, size_t len)
{
    co_return co_await current_scheduler()->async_read(fd_, buf, len);
}

Task<ssize_t> TcpClient::write(const void * buf, size_t len)
{
    co_return co_await current_scheduler()->async_write(fd_, buf, len);
}

void TcpClient::close()
{
    if (fd_ >= 0)
    {
        ::close(fd_);
        fd_ = -1;
    }
}

} // namespace my_coro
