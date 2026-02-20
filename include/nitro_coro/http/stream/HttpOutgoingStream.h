/**
 * @file HttpOutgoingStream.h
 * @brief HTTP outgoing stream for writing requests and responses
 */
#pragma once

#include <nitro_coro/http/HttpRequestAccessor.h>
#include <nitro_coro/http/HttpResponseAccessor.h>
#include <nitro_coro/http/stream/HttpOutgoingStreamBase.h>

namespace nitro_coro::http
{

// Forward declaration
template <typename T>
class HttpOutgoingStream;

// ============================================================================
// HttpOutgoingStream<HttpRequest> - Write HTTP Request
// ============================================================================

template <>
class HttpOutgoingStream<HttpRequest>
    : public HttpOutgoingStreamBase<HttpOutgoingStream<HttpRequest>, HttpRequest>
{
public:
    explicit HttpOutgoingStream(net::TcpConnectionPtr conn)
        : HttpOutgoingStreamBase(std::move(conn)) {}

    void setMethod(const std::string & method) { data_.method = method; }
    void setPath(const std::string & path) { data_.path = path; }
    void setVersion(const std::string & version) { data_.version = version; }

    Task<> writeHeaders();
};

// ============================================================================
// HttpOutgoingStream<HttpResponse> - Write HTTP Response
// ============================================================================

template <>
class HttpOutgoingStream<HttpResponse>
    : public HttpOutgoingStreamBase<HttpOutgoingStream<HttpResponse>, HttpResponse>
{
public:
    explicit HttpOutgoingStream(net::TcpConnectionPtr conn)
        : HttpOutgoingStreamBase(std::move(conn)) {}

    void setStatus(int code, const std::string & reason = "");
    void setVersion(const std::string & version) { data_.version = version; }

    Task<> writeHeaders();

private:
    static const char * getDefaultReason(int code);
};

} // namespace nitro_coro::http
