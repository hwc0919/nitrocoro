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

class HttpCompleteRequest : protected HttpRequest, public HttpRequestAccessor<HttpCompleteRequest>
{
    template <typename, typename>
    friend class HttpDataAccessor;

public:
    const std::string & getBody() const { return body_; }

protected:
    std::string body_;
    const HttpRequest & getData() const { return *this; }
};

class HttpCompleteResponse : protected HttpResponse, public HttpResponseAccessor<HttpCompleteResponse>
{
    template <typename, typename>
    friend class HttpDataAccessor;

public:
    HttpCompleteResponse() = default;
    HttpCompleteResponse(HttpResponse && resp, std::string && b)
        : HttpResponse(std::move(resp)), body_(std::move(b)) {}

    const std::string & getBody() const { return body_; }

protected:
    std::string body_;
    const HttpResponse & getData() const { return *this; }
};

} // namespace nitro_coro::http
