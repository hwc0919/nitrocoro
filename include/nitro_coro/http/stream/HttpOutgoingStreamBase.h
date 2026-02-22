/**
 * @file HttpOutgoingStreamBase.h
 * @brief Base class for HTTP outgoing streams
 */
#pragma once
#include <nitro_coro/core/Task.h>
#include <nitro_coro/net/TcpConnection.h>

#include <string>
#include <string_view>

namespace nitro_coro::http
{

template <typename DataType>
class HttpOutgoingStreamBase
{
public:
    void setHeader(const std::string & name, const std::string & value);
    void setHeader(HttpHeader header);
    void setCookie(const std::string & name, const std::string & value);
    Task<> write(const char * data, size_t len);
    Task<> write(std::string_view data);
    Task<> end();
    Task<> end(std::string_view data);
    Task<> writeHeaders();

protected:
    explicit HttpOutgoingStreamBase(net::TcpConnectionPtr conn)
        : conn_(std::move(conn)) {}

    static const char * getDefaultReason(int code);

    DataType data_;
    net::TcpConnectionPtr conn_;
    bool headersSent_ = false;
};

} // namespace nitro_coro::http
