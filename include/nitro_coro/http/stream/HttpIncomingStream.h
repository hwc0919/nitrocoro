/**
 * @file HttpIncomingStream.h
 * @brief HTTP incoming stream for reading requests and responses
 */
#pragma once

#include <nitro_coro/http/stream/HttpIncomingStreamBase.h>

namespace nitro_coro::http
{

// Forward declaration
template <typename T>
class HttpIncomingStream;

// ============================================================================
// HttpIncomingStream<HttpRequest> - Read HTTP Request
// ============================================================================

template <>
class HttpIncomingStream<HttpRequest>
    : public HttpIncomingStreamBase<HttpIncomingStream<HttpRequest>, HttpRequest>
{
    using Base = HttpIncomingStreamBase<HttpIncomingStream<HttpRequest>, HttpRequest>;

public:
    HttpIncomingStream() = default;
    explicit HttpIncomingStream(net::TcpConnectionPtr conn)
        : Base(std::move(conn)) {}

    // Parse from buffer
    int parse(const char * data, size_t len);

    // Request-specific accessors
    const std::string & method() const { return data_.method; }
    const std::string & path() const { return data_.path; }
    const std::string & version() const { return data_.version; }
    std::string_view query(const std::string & name) const;

private:
    friend class HttpServer;

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
    : public HttpIncomingStreamBase<HttpIncomingStream<HttpResponse>, HttpResponse>
{
    using Base = HttpIncomingStreamBase<HttpIncomingStream<HttpResponse>, HttpResponse>;

public:
    explicit HttpIncomingStream(net::TcpConnectionPtr conn)
        : Base(std::move(conn)) {}

    // Parse from buffer
    int parse(const char * data, size_t len);

    // Response-specific accessors
    int statusCode() const { return data_.statusCode; }
    const std::string & statusReason() const { return data_.statusReason; }
    const std::string & version() const { return data_.version; }

private:
    void parseStatusLine(std::string_view line);
    void parseHeader(std::string_view line);
    void parseCookies(const std::string & cookieHeader);
};

} // namespace nitro_coro::http
