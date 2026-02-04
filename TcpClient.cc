/**
 * @file TcpClient.cc
 * @brief Implementation of coroutine-based TCP client
 */
#include "TcpClient.h"
#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include <cstring>
#include <stdexcept>

namespace my_coro
{

TcpClient::TcpClient() : fd_(-1)
{
}

TcpClient::~TcpClient()
{
    close();
}

Task TcpClient::connect(const char* host, int port)
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

    ::connect(fd_, (sockaddr*)&addr, sizeof(addr));
    co_await current_scheduler()->sleep_for(0.1);
}

Task TcpClient::read(void* buf, size_t len, ssize_t* result)
{
    *result = co_await current_scheduler()->async_read(fd_, buf, len);
}

Task TcpClient::write(const void* buf, size_t len, ssize_t* result)
{
    *result = co_await current_scheduler()->async_write(fd_, buf, len);
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
