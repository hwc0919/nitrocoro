/**
 * @file HttpOutgoingStreamBase.h
 * @brief CRTP base class for HTTP outgoing streams
 */
#pragma once
#include <nitro_coro/core/Task.h>
#include <nitro_coro/net/TcpConnection.h>

#include <string>
#include <string_view>

namespace nitro_coro::http
{

template <typename Derived, typename DataType>
class HttpOutgoingStreamBase
{
protected:
    DataType data_;
    net::TcpConnectionPtr conn_;
    bool headersSent_ = false;

    explicit HttpOutgoingStreamBase(net::TcpConnectionPtr conn)
        : conn_(std::move(conn)) {}

public:
    void setHeader(const std::string & name, const std::string & value);
    void setHeader(HttpHeader header);
    void setCookie(const std::string & name, const std::string & value);
    Task<> write(const char * data, size_t len);
    Task<> write(std::string_view data);
    Task<> end();
    Task<> end(std::string_view data);
};

} // namespace nitro_coro::http
