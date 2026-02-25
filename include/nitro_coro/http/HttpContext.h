/**
 * @file HttpContext.h
 * @brief HTTP context for parsing headers
 */
#pragma once
#include <nitro_coro/core/Task.h>
#include <nitro_coro/http/HttpParser.h>
#include <nitro_coro/net/TcpConnection.h>
#include <nitro_coro/utils/StringBuffer.h>

#include <memory>

namespace nitro_coro::http
{

template <typename MessageType>
class HttpContext
{
public:
    HttpContext(net::TcpConnectionPtr conn, std::shared_ptr<utils::StringBuffer> buffer)
        : conn_(std::move(conn)), buffer_(std::move(buffer))
    {
    }

    Task<MessageType> receiveMessage();

    net::TcpConnectionPtr connection() const { return conn_; }
    std::shared_ptr<utils::StringBuffer> buffer() const { return buffer_; }

private:
    net::TcpConnectionPtr conn_;
    std::shared_ptr<utils::StringBuffer> buffer_;
};

} // namespace nitro_coro::http
