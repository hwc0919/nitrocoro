/**
 * @file IoChannel.h
 * @brief IoChannel abstraction for managing fd I/O operations with multiple concurrent readers/writers
 */
#pragma once

#include "Task.h"
#include <coroutine>
#include <memory>
#include <queue>

namespace my_coro
{

class CoroScheduler;

class IoChannel
{
public:
    IoChannel(int fd, CoroScheduler * scheduler);
    ~IoChannel();

    IoChannel(const IoChannel &) = delete;
    IoChannel & operator=(const IoChannel &) = delete;
    IoChannel(IoChannel &&) = delete;
    IoChannel & operator=(IoChannel &&) = delete;

    Task<ssize_t> read(void * buf, size_t len);
    Task<ssize_t> write(const void * buf, size_t len);

private:
    int fd_{ -1 };
    CoroScheduler * scheduler_{ nullptr };
};

} // namespace my_coro
