/**
 * @file BodyReader.h
 * @brief Body reader interface and factory
 */
#pragma once
#include <nitro_coro/core/Task.h>
#include <nitro_coro/http/HttpParser.h>
#include <nitro_coro/net/TcpConnection.h>
#include <nitro_coro/utils/StringBuffer.h>

#include <memory>
#include <string_view>

namespace nitro_coro::http
{

class BodyReader
{
public:
    virtual ~BodyReader() = default;

    virtual Task<std::string_view> read(size_t maxSize) = 0;
    virtual Task<size_t> readTo(char * buf, size_t len) = 0;
    virtual Task<std::string_view> readAll() = 0;
    virtual bool isComplete() const = 0;

    static std::unique_ptr<BodyReader> create(
        TransferMode mode,
        net::TcpConnectionPtr conn,
        std::shared_ptr<utils::StringBuffer> buffer,
        size_t contentLength);
};

} // namespace nitro_coro::http
