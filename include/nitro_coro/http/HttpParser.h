/**
 * @file HttpParser.h
 * @brief HTTP parser template and specializations
 */
#pragma once
#include <nitro_coro/http/HttpMessage.h>
#include <string>
#include <string_view>

namespace nitro_coro::http
{

template <typename DataType>
class HttpParser;

// ============================================================================
// HttpParser<HttpRequest> - Parse HTTP Request
// ============================================================================

template <>
class HttpParser<HttpRequest>
{
public:
    explicit HttpParser(HttpRequest & data)
        : data_(data) {}

    bool parseLine(std::string_view line);

    bool isHeaderComplete() const { return headerComplete_; }
    size_t contentLength() const { return contentLength_; }

private:
    HttpRequest & data_;
    bool headerComplete_ = false;
    size_t contentLength_ = 0;

    void parseRequestLine(std::string_view line);
    void parseHeader(std::string_view line);
    void parseQueryString();
    void parseCookies(const std::string & cookieHeader);
};

// ============================================================================
// HttpParser<HttpResponse> - Parse HTTP Response
// ============================================================================

template <>
class HttpParser<HttpResponse>
{
private:
    HttpResponse & data_;
    bool headerComplete_ = false;
    size_t contentLength_ = 0;

    void parseStatusLine(std::string_view line);
    void parseHeader(std::string_view line);
    void parseCookies(const std::string & cookieHeader);

public:
    explicit HttpParser(HttpResponse & data)
        : data_(data) {}

    bool parseLine(std::string_view line);

    bool isHeaderComplete() const { return headerComplete_; }
    size_t contentLength() const { return contentLength_; }
};

} // namespace nitro_coro::http
