/**
 * @file HttpStream.h
 * @brief Template-based HTTP streaming for requests and responses
 */
#pragma once

#include <nitro_coro/core/Task.h>
#include <nitro_coro/http/HttpMessage.h>
#include <nitro_coro/net/TcpConnection.h>
#include <string>
#include <string_view>

namespace nitro_coro::http
{

// Forward declarations
template <typename T>
class HttpIncomingStream;

template <typename T>
class HttpOutgoingStream;

// ============================================================================
// HttpIncomingStream<HttpRequest> - Read HTTP Request
// ============================================================================

template <>
class HttpIncomingStream<HttpRequest>
{
public:
    HttpIncomingStream() = default;
    explicit HttpIncomingStream(net::TcpConnectionPtr conn)
        : conn_(std::move(conn)) {}

    // Parse from buffer
    int parse(const char * data, size_t len);
    bool isHeaderComplete() const { return headerComplete_; }
    bool isComplete() const { return complete_; }

    // Request-specific accessors
    const std::string & method() const { return data_.method; }
    const std::string & path() const { return data_.path; }
    const std::string & version() const { return data_.version; }
    std::string_view query(const std::string & name) const;

    // Common accessors
    std::string_view header(HttpHeader::NameCode code) const;
    std::string_view header(const std::string & name) const;
    const std::map<std::string, HttpHeader> & headers() const { return data_.headers; }
    std::string_view cookie(const std::string & name) const;
    const std::map<std::string, std::string> & cookies() const { return data_.cookies; }

    // Body streaming
    bool hasBody() const { return contentLength_ > 0; }
    size_t contentLength() const { return contentLength_; }
    Task<std::string_view> read(size_t maxSize = 4096);
    Task<std::string_view> readAll();
    Task<size_t> readTo(char * buf, size_t len);

private:
    friend class HttpServer;

    void parseRequestLine(std::string_view line);
    void parseHeader(std::string_view line);
    void parseQueryString();
    void parseCookies(const std::string & cookieHeader);

    HttpRequest data_;
    net::TcpConnectionPtr conn_;

    std::string buffer_;
    bool headerComplete_ = false;
    bool complete_ = false;
    size_t contentLength_ = 0;
    size_t bodyBytesRead_ = 0;
};

// ============================================================================
// HttpIncomingStream<HttpResponse> - Read HTTP Response
// ============================================================================
template <>
class HttpIncomingStream<HttpResponse>
{
public:
    HttpIncomingStream() = default;
    explicit HttpIncomingStream(net::TcpConnectionPtr conn)
        : conn_(std::move(conn)) {}

    // Parse from buffer
    int parse(const char * data, size_t len);
    bool isHeaderComplete() const { return headerComplete_; }
    bool isComplete() const { return complete_; }

    // Response-specific accessors
    int statusCode() const { return data_.statusCode; }
    const std::string & statusReason() const { return data_.statusReason; }
    const std::string & version() const { return data_.version; }

    // Common accessors
    std::string_view header(const std::string & name) const;
    const std::map<std::string, HttpHeader> & headers() const { return data_.headers; }
    std::string_view cookie(const std::string & name) const;
    const std::map<std::string, std::string> & cookies() const { return data_.cookies; }

    // Body streaming
    bool hasBody() const { return contentLength_ > 0; }
    size_t contentLength() const { return contentLength_; }
    Task<std::string_view> read(size_t maxSize = 4096);
    Task<std::string_view> readAll();
    Task<size_t> readTo(char * buf, size_t len);

private:
    friend class HttpClient;

    void parseStatusLine(std::string_view line);
    void parseHeader(std::string_view line);
    void parseCookies(const std::string & cookieHeader);

    HttpResponse data_;
    net::TcpConnectionPtr conn_;

    std::string buffer_;
    bool headerComplete_ = false;
    bool complete_ = false;
    size_t contentLength_ = 0;
    size_t bodyBytesRead_ = 0;
};

// ============================================================================
// HttpOutgoingStream<HttpRequest> - Write HTTP Request
// ============================================================================
template <>
class HttpOutgoingStream<HttpRequest>
{
public:
    explicit HttpOutgoingStream(net::TcpConnectionPtr conn)
        : conn_(std::move(conn)) {}

    // Request-specific setters
    void setMethod(const std::string & method) { data_.method = method; }
    void setPath(const std::string & path) { data_.path = path; }
    void setVersion(const std::string & version) { data_.version = version; }

    // Common setters
    void setHeader(const std::string & name, const std::string & value);
    void setCookie(const std::string & name, const std::string & value);

    // Body streaming
    Task<> write(const char * data, size_t len);
    Task<> write(std::string_view data);
    Task<> end();
    Task<> end(std::string_view data);

private:
    Task<> writeHeaders();

    HttpRequest data_;
    net::TcpConnectionPtr conn_;
    bool headersSent_ = false;
};

// ============================================================================
// HttpOutgoingStream<HttpResponse> - Write HTTP Response
// ============================================================================
template <>
class HttpOutgoingStream<HttpResponse>
{
public:
    explicit HttpOutgoingStream(net::TcpConnectionPtr conn)
        : conn_(std::move(conn)) {}

    // Response-specific setters
    void setStatus(int code, const std::string & reason = "");
    void setVersion(const std::string & version) { data_.version = version; }

    // Common setters
    void setHeader(HttpHeader header);
    void setHeader(const std::string & name, const std::string & value);
    void setCookie(const std::string & name, const std::string & value);

    // Body streaming
    Task<> write(const char * data, size_t len);
    Task<> write(std::string_view data);
    Task<> end();
    Task<> end(std::string_view data);

private:
    Task<> writeHeaders();
    static const char * getDefaultReason(int code);

    HttpResponse data_;
    net::TcpConnectionPtr conn_;
    bool headersSent_ = false;
};

} // namespace nitro_coro::http
