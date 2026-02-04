/**
 * @file TcpClient.h
 * @brief Coroutine-based TCP client component
 */
#pragma once

#include "CoroScheduler.h"

namespace my_coro
{

class TcpClient
{
public:
    TcpClient();
    ~TcpClient();

    Task connect(const char* host, int port);
    Task read(void* buf, size_t len, ssize_t* result);
    Task write(const void* buf, size_t len, ssize_t* result);
    void close();
    
    int fd() const { return fd_; }
    bool is_connected() const { return fd_ >= 0; }

private:
    int fd_;
};

} // namespace my_coro
