/**
 * @file HttpMessage.h
 * @brief HTTP message data structures
 */
#pragma once

#include <nitro_coro/http/HttpDataAccessor.h>
#include <nitro_coro/http/HttpHeader.h>
#include <nitro_coro/http/HttpTypes.h>

#include <map>
#include <string>

namespace nitro_coro::http
{

struct HttpRequest
{
    std::string method;
    std::string path;
    Version version = Version::kHttp11;
    std::map<std::string, HttpHeader, std::less<>> headers;
    std::map<std::string, std::string, std::less<>> cookies;
    std::map<std::string, std::string, std::less<>> queries;
};

struct HttpResponse
{
    StatusCode statusCode = StatusCode::k200OK;
    std::string statusReason;
    Version version = Version::kHttp11;
    std::map<std::string, HttpHeader, std::less<>> headers;
    std::map<std::string, std::string, std::less<>> cookies;
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

} // namespace nitro_coro::http
