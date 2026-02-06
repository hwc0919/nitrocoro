/**
 * @file IoChannel.cc
 * @brief Implementation of IoChannel
 */
#include "IoChannel.h"
#include "CoroScheduler.h"

namespace my_coro
{

IoChannel::IoChannel(int fd, CoroScheduler * scheduler)
    : fd_(fd), scheduler_(scheduler)
{
}

IoChannel::~IoChannel()
{
}

Task<ssize_t> IoChannel::read(void * buf, size_t len)
{
    co_return co_await scheduler_->async_read(fd_, buf, len);
}

Task<ssize_t> IoChannel::write(const void * buf, size_t len)
{
    co_return co_await scheduler_->async_write(fd_, buf, len);
}

} // namespace my_coro
