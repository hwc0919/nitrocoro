/**
 * @file TcpConnection.h
 * @brief RAII wrapper for TCP connection file descriptor
 */
#pragma once

#include "Task.h"

namespace my_coro
{

class TcpConnection
{
public:
    explicit TcpConnection(int fd);
    ~TcpConnection();

    TcpConnection(const TcpConnection &) = delete;
    TcpConnection & operator=(const TcpConnection &) = delete;
    TcpConnection(TcpConnection &&) = delete;
    TcpConnection & operator=(TcpConnection &&) = delete;

    Task<ssize_t> read(void * buf, size_t len);
    Task<ssize_t> write(const void * buf, size_t len);
    bool is_open() const { return fd_ >= 0; }

private:
    int fd_;
};

} // namespace my_coro
