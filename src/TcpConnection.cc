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

TcpConnection::TcpConnection(std::unique_ptr<IoChannel> channelPtr)
    : fd_(channelPtr->fd())
    , ioChannelPtr_(std::move(channelPtr))
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
