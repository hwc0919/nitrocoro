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
    HttpParser() = default;

    bool parseLine(std::string_view line);

    bool isHeaderComplete() const { return headerComplete_; }
    HttpRequest && extractMessage() { return std::move(data_); }

private:
    HttpRequest data_;
    bool headerComplete_ = false;

    void parseRequestLine(std::string_view line);
    void parseHeader(std::string_view line);
    void parseQueryString(std::string_view queryStr);
    void parseCookies(const std::string & cookieHeader);
    void processHeaders();
    void processTransferMode();
    void processKeepAlive();
};

// ============================================================================
// HttpParser<HttpResponse> - Parse HTTP Response
// ============================================================================

template <>
class HttpParser<HttpResponse>
{
public:
    HttpParser() = default;

    bool parseLine(std::string_view line);

    bool isHeaderComplete() const { return headerComplete_; }
    HttpResponse && extractMessage() { return std::move(data_); }

private:
    HttpResponse data_;
    bool headerComplete_ = false;

    void parseStatusLine(std::string_view line);
    void parseHeader(std::string_view line);
    void parseCookies(const std::string & cookieHeader);
    void processHeaders();
    void processTransferMode();
    void processConnectionClose();
};

} // namespace nitro_coro::http
