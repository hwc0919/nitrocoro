/**
 * @file HttpContext.h
 * @brief HTTP context for parsing headers
 */
#pragma once
#include <nitrocoro/core/Task.h>
#include <nitrocoro/http/HttpParser.h>
#include <nitrocoro/net/TcpConnection.h>
#include <nitrocoro/utils/StringBuffer.h>

#include <memory>

namespace nitrocoro::http
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

} // namespace nitrocoro::http
