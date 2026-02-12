/**
 * @file TcpConnection.cc
 * @brief Implementation of TcpConnection
 */
#include "TcpConnection.h"
#include "Scheduler.h"
#include <iostream>
#include <unistd.h>

namespace my_coro
{

TcpConnection::TcpConnection(int fd)
    : fd_(fd)
    , ioChannelPtr_(new IoChannel(fd, Scheduler::current()))
{
}

TcpConnection::~TcpConnection() = default;

Task<ssize_t> TcpConnection::read(void * buf, size_t len)
{
    BufferReader reader(buf, len);
    co_await ioChannelPtr_->performRead(&reader);
    co_return reader.readLen();
}

Task<> TcpConnection::write(const void * buf, size_t len)
{
    BufferWriter writer(buf, len);
    co_await ioChannelPtr_->performWrite(&writer);
    co_return;
}

} // namespace my_coro
