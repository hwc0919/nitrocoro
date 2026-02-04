/**
 * @file TcpClient.h
 * @brief Coroutine-based TCP client component
 */
#pragma once

#include "Task.h"

namespace my_coro
{

class TcpClient
{
public:
    TcpClient();
    ~TcpClient();

    Task<> connect(const char * host, int port);
    Task<ssize_t> read(void * buf, size_t len);
    Task<ssize_t> write(const void * buf, size_t len);
    void close();

    bool is_connected() const { return fd_ >= 0; }

private:
    int fd_;
};

} // namespace my_coro
