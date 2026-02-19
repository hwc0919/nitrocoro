/**
 * @file HttpOutgoingStreamBase.h
 * @brief CRTP base class for HTTP outgoing streams
 */
#pragma once

#include <nitro_coro/core/Task.h>
#include <nitro_coro/http/HttpMessage.h>
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
    void setHeader(const std::string & name, const std::string & value)
    {
        HttpHeader header(name, value);
        data_.headers.insert_or_assign(header.name(), std::move(header));
    }

    void setHeader(HttpHeader header)
    {
        data_.headers.insert_or_assign(header.name(), std::move(header));
    }

    void setCookie(const std::string & name, const std::string & value)
    {
        data_.cookies[name] = value;
    }

    Task<> write(const char * data, size_t len)
    {
        co_await static_cast<Derived *>(this)->writeHeaders();
        co_await conn_->write(data, len);
    }

    Task<> write(std::string_view data)
    {
        co_await write(data.data(), data.size());
    }

    Task<> end()
    {
        co_await static_cast<Derived *>(this)->writeHeaders();
    }

    Task<> end(std::string_view data)
    {
        co_await write(data);
    }
};

} // namespace nitro_coro::http
