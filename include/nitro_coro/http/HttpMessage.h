/**
 * @file HttpMessage.h
 * @brief HTTP message data structures
 */
#pragma once

#include <map>
#include <nitro_coro/http/HttpDataAccessor.h>
#include <nitro_coro/http/HttpHeader.h>
#include <string>

namespace nitro_coro::http
{

struct HttpRequest
{
    std::string method;
    std::string path;
    std::string version = "HTTP/1.1";
    std::map<std::string, HttpHeader> headers;
    std::map<std::string, std::string> cookies;
    std::map<std::string, std::string> queries;
};

struct HttpResponse
{
    int statusCode = 200;
    std::string statusReason;
    std::string version = "HTTP/1.1";
    std::map<std::string, HttpHeader> headers;
    std::map<std::string, std::string> cookies;
};

class HttpCompleteRequest : public HttpRequestAccessor<HttpCompleteRequest>
{
    template <typename, typename>
    friend class HttpDataAccessor;

public:
    HttpCompleteRequest() = default;
    HttpCompleteRequest(HttpRequest && req, std::string && b)
        : request_(std::move(req)), body_(std::move(b)) {}

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
    HttpCompleteResponse(HttpResponse && resp, std::string && b)
        : response_(std::move(resp)), body_(std::move(b)) {}

    const std::string & body() const { return body_; }

protected:
    HttpResponse response_;
    std::string body_;
    const HttpResponse & getData() const { return response_; }
};

} // namespace nitro_coro::http
