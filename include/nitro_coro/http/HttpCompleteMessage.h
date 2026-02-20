/**
 * @file HttpCompleteMessage.h
 * @brief Complete HTTP messages with body
 */
#pragma once

#include <nitro_coro/http/HttpMessage.h>
#include <nitro_coro/http/HttpRequestAccessor.h>
#include <nitro_coro/http/HttpResponseAccessor.h>
#include <string>

namespace nitro_coro::http
{

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
