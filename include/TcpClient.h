/**
 * @file TcpClient.h
 * @brief Coroutine-based TCP client component
 */
#pragma once

#include "IoChannel.h"
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
    Task<> write(const void * buf, size_t len);
    void close();

    bool is_connected() const { return fd_ >= 0; }

private:
    int fd_;
    std::unique_ptr<IoChannel> ioChannelPtr_;
};

} // namespace my_coro
