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
    : public HttpRequestAccessor<HttpIncomingStream<HttpRequest>>, public HttpIncomingStreamBase<HttpIncomingStream<HttpRequest>, HttpRequest>
{
public:
    explicit HttpIncomingStream(net::TcpConnectionPtr conn)
        : HttpIncomingStreamBase(std::move(conn)) {}

    Task<> readAndParse();

private:
    void parseRequestLine(std::string_view line);
    void parseHeader(std::string_view line);
    void parseQueryString();
    void parseCookies(const std::string & cookieHeader);
};

// ============================================================================
// HttpIncomingStream<HttpResponse> - Read HTTP Response
// ============================================================================

template <>
class HttpIncomingStream<HttpResponse>
    : public HttpResponseAccessor<HttpIncomingStream<HttpResponse>>, public HttpIncomingStreamBase<HttpIncomingStream<HttpResponse>, HttpResponse>
{
public:
    explicit HttpIncomingStream(net::TcpConnectionPtr conn)
        : HttpIncomingStreamBase(std::move(conn)) {}

    Task<> readAndParse();
    Task<HttpCompleteResponse> toCompleteResponse();

private:
    void parseStatusLine(std::string_view line);
    void parseHeader(std::string_view line);
    void parseCookies(const std::string & cookieHeader);
};

} // namespace nitro_coro::http
