/**
 * @file HttpMessage.h
 * @brief HTTP message data structures
 */
#pragma once

#include <nitrocoro/http/HttpDataAccessor.h>
#include <nitrocoro/http/HttpHeader.h>
#include <nitrocoro/http/HttpTypes.h>

#include <map>
#include <string>

namespace nitrocoro::http
{

using HttpHeaderMap = std::map<std::string, HttpHeader, std::less<>>;
using HttpCookieMap = std::map<std::string, std::string, std::less<>>;
using HttpQueryMap = std::map<std::string, std::string, std::less<>>;

struct HttpRequest
{
    std::string method;
    std::string fullPath;
    std::string path;
    std::string query;
    Version version = Version::kHttp11;
    HttpHeaderMap headers;
    HttpCookieMap cookies;
    HttpQueryMap queries;

    // Metadata parsed from headers
    TransferMode transferMode = TransferMode::UntilClose;
    size_t contentLength = 0;
    bool keepAlive = false;
};

struct HttpResponse
{
    StatusCode statusCode = StatusCode::k200OK;
    std::string statusReason;
    Version version = Version::kHttp11;
    HttpHeaderMap headers;
    HttpCookieMap cookies;

    // Metadata for sending
    TransferMode transferMode = TransferMode::ContentLength;
    size_t contentLength = 0;
    bool shouldClose = false;
};

class HttpCompleteRequest : public HttpRequestAccessor<HttpCompleteRequest>
{
    template <typename, typename>
    friend class HttpDataAccessor;

public:
    HttpCompleteRequest() = default;
    HttpCompleteRequest(HttpRequest && req, std::string && body)
        : request_(std::move(req)), body_(std::move(body)) {}

    const std::string & body() const { return body_; }

protected:
    HttpRequest request_;
    std::string body_;
    const HttpRequest & getData() const { return request_; }
};

class HttpCompleteResponse : public HttpResponseAccessor<HttpCompleteResponse>
{
    template <typename, typename>
    friend class HttpDataAccessor;

public:
    HttpCompleteResponse() = default;
    HttpCompleteResponse(HttpResponse && resp, std::string && body)
        : response_(std::move(resp)), body_(std::move(body)) {}

    const std::string & body() const { return body_; }

protected:
    HttpResponse response_;
    std::string body_;
    const HttpResponse & getData() const { return response_; }
};

} // namespace nitrocoro::http
