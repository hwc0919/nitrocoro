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

Task<ssize_t> TcpConnection::read(void * buf, size_t len)
{
    co_return co_await CoroScheduler::current()->async_read(fd_, buf, len);
}

Task<ssize_t> TcpConnection::write(const void * buf, size_t len)
{
    co_return co_await CoroScheduler::current()->async_write(fd_, buf, len);
}

} // namespace my_coro
