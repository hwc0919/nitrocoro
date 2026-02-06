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
    scheduler_->registerIoChannel(this);
}

IoChannel::~IoChannel()
{
    // TODO: clear all pending handles
    scheduler_->unregisterIoChannel(this);
}

Task<ssize_t> IoChannel::read(void * buf, size_t len)
{
    ssize_t result = co_await ReadAwaiter{
        .channel_ = this,
        .buf_ = buf,
        .len_ = len
    };

    co_return result;
}

Task<ssize_t> IoChannel::write(const void * buf, size_t len)
{
    ssize_t result = co_await WriteAwaiter{
        .channel_ = this,
        .buf_ = buf,
        .len_ = len
    };

    co_return result;
}

void IoChannel::handleReadable()
{
    assert(readable_ == false);
    readable_ = true;
    if (pendingRead_)
    {
        auto h = pendingRead_;
        pendingRead_ = nullptr;
        scheduler_->schedule(h);
    }
}

void IoChannel::handleWritable()
{
    assert(writable_ == false);
    writable_ = true;
    if (pendingWrite_)
    {
        auto h = pendingWrite_;
        pendingWrite_ = nullptr;
        scheduler_->schedule(h);
    }
}

} // namespace my_coro
