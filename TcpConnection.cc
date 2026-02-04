/**
 * @file TcpConnection.cc
 * @brief Implementation of TcpConnection
 */
#include "TcpConnection.h"
#include "CoroScheduler.h"
#include <iostream>
#include <unistd.h>

namespace my_coro
{

TcpConnection::TcpConnection(int fd) : fd_(fd)
{
}

TcpConnection::~TcpConnection()
{
    if (fd_ >= 0)
    {
        std::cout << "Closing connection: fd=" << fd_ << "\n";
        close(fd_);
    }
}

Task<> TcpConnection::read(void * buf, size_t len, ssize_t * result)
{
    *result = co_await current_scheduler() -> async_read(fd_, buf, len);
}

Task<> TcpConnection::write(const void * buf, size_t len, ssize_t * result)
{
    *result = co_await current_scheduler() -> async_write(fd_, buf, len);
}

} // namespace my_coro
