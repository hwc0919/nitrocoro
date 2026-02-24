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

enum class TransferMode
{
    ContentLength,
    Chunked,
    UntilClose
};

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
    size_t contentLength() const { return contentLength_; }
    TransferMode transferMode() const { return transferMode_; }
    HttpRequest && extractMessage() { return std::move(data_); }

private:
    HttpRequest data_;
    bool headerComplete_ = false;
    size_t contentLength_ = 0;
    TransferMode transferMode_ = TransferMode::UntilClose;

    void parseRequestLine(std::string_view line);
    void parseHeader(std::string_view line);
    void parseQueryString(std::string_view queryStr);
    void parseCookies(const std::string & cookieHeader);
    void processHeaders();
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
    size_t contentLength() const { return contentLength_; }
    TransferMode transferMode() const { return transferMode_; }
    HttpResponse && extractMessage() { return std::move(data_); }

private:
    HttpResponse data_;
    bool headerComplete_ = false;
    size_t contentLength_ = 0;
    TransferMode transferMode_ = TransferMode::UntilClose;

    void parseStatusLine(std::string_view line);
    void parseHeader(std::string_view line);
    void parseCookies(const std::string & cookieHeader);
    void processHeaders();
};

} // namespace nitro_coro::http
