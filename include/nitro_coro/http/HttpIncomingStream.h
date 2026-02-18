/**
 * @file HttpIncomingStream.h
 * @brief HTTP incoming message stream (for reading requests/responses)
 */
#pragma once

#include <map>
#include <memory>
#include <nitro_coro/core/Task.h>
#include <nitro_coro/http/HttpHeader.h>
#include <nitro_coro/net/TcpConnection.h>
#include <string>
#include <string_view>

namespace nitro_coro::http
{

class HttpIncomingStream
{
public:
    HttpIncomingStream() = default;

    // Parse HTTP message from buffer, returns bytes consumed or -1 on error
    int parse(const char * data, size_t len);

    bool isHeaderComplete() const { return state_ >= State::Body; }
    bool isComplete() const { return complete_; }

    // Metadata access (available after headers parsed)
    const std::string & getMethod() const { return method_; }
    const std::string & getUrl() const { return path_; }
    const std::string & getVersion() const { return version_; }

    std::string_view getHeader(const std::string & name) const;
    const std::map<std::string, HttpHeader> & getHeaders() const { return headers_; }

    std::string_view getCookie(const std::string & name) const;
    const std::map<std::string, std::string> & getCookies() const { return cookies_; }

    std::string_view getQuery(const std::string & name) const;

    // Body metadata
    bool hasBody() const { return contentLength_ > 0; }
    size_t getContentLength() const { return contentLength_; }

    // Stream reading body (can be mixed)
    Task<std::string_view> read(size_t maxSize = 4096);
    Task<std::string_view> readAll();

    // Read to external buffer (cannot be mixed with read/readAll)
    Task<size_t> readTo(char * buf, size_t len);

private:
    friend class HttpServer;

    enum class State
    {
        RequestLine,
        Headers,
        Body,
        Complete
    };

    void parseRequestLine(std::string_view line);
    void parseHeader(std::string_view line);
    void parseQueryString();
    void parseCookies(const std::string & cookieHeader);

    State state_ = State::RequestLine;
    bool complete_ = false;

    std::string method_;
    std::string path_;
    std::string version_;
    std::map<std::string, HttpHeader> headers_;
    std::map<std::string, std::string> cookies_;
    std::map<std::string, std::string> queries_;

    std::string buffer_;
    size_t bodyBytesRead_ = 0;
    size_t contentLength_ = 0;

    net::TcpConnectionPtr conn_; // For streaming body reads
};

} // namespace nitro_coro::http
