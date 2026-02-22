/**
 * @file HttpIncomingStream.h
 * @brief HTTP incoming stream for reading requests and responses
 */
#pragma once
#include <nitro_coro/http/HttpDataAccessor.h>
#include <nitro_coro/http/HttpMessage.h>
#include <nitro_coro/http/stream/HttpIncomingStreamBase.h>

namespace nitro_coro::http
{

// Forward declaration
template <typename T>
class HttpIncomingStream;

class HttpCompleteResponse;

// ============================================================================
// HttpIncomingStream<HttpRequest> - Read HTTP Request
// ============================================================================

template <>
class HttpIncomingStream<HttpRequest>
    : public HttpRequestAccessor<HttpIncomingStream<HttpRequest>>,
      public HttpIncomingStreamBase<HttpRequest>
{
public:
    explicit HttpIncomingStream(net::TcpConnectionPtr conn)
        : HttpIncomingStreamBase(std::move(conn)) {}
};

// ============================================================================
// HttpIncomingStream<HttpResponse> - Read HTTP Response
// ============================================================================

template <>
class HttpIncomingStream<HttpResponse>
    : public HttpResponseAccessor<HttpIncomingStream<HttpResponse>>,
      public HttpIncomingStreamBase<HttpResponse>
{
public:
    explicit HttpIncomingStream(net::TcpConnectionPtr conn)
        : HttpIncomingStreamBase(std::move(conn)) {}

    Task<HttpCompleteResponse> toCompleteResponse();
};

} // namespace nitro_coro::http
