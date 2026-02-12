/**
 * @file TcpClient.h
 * @brief Coroutine-based TCP client component
 */
#pragma once

#include "IoChannel.h"
#include "Task.h"
#include "TcpConnection.h"

namespace my_coro
{

class TcpClient
{
public:
    TcpClient();
    ~TcpClient();

    Task<TcpConnectionPtr> connect(const char * host, int port);
};

} // namespace my_coro
