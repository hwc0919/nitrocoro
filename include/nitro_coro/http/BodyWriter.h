/**
 * @file BodyWriter.h
 * @brief Body writer interface and factory
 */
#pragma once
#include <nitro_coro/core/Task.h>
#include <nitro_coro/http/HttpParser.h>
#include <nitro_coro/net/TcpConnection.h>

#include <memory>
#include <string_view>

namespace nitro_coro::http
{

class BodyWriter
{
public:
    virtual ~BodyWriter() = default;

    virtual Task<> write(std::string_view data) = 0;
    virtual Task<> end() = 0;

    static std::unique_ptr<BodyWriter> create(
        TransferMode mode,
        net::TcpConnectionPtr conn,
        size_t contentLength = 0);
};

} // namespace nitro_coro::http
