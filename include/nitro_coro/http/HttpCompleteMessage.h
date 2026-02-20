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
    std::string body;

    const std::string & getBody() const { return body; }

protected:
    const HttpRequest & getData() const { return *this; }
};

class HttpCompleteResponse : protected HttpResponse, public HttpResponseAccessor<HttpCompleteResponse>
{
    template <typename, typename>
    friend class HttpDataAccessor;

public:
    std::string body;

    const std::string & getBody() const { return body; }

protected:
    const HttpResponse & getData() const { return *this; }
};

} // namespace nitro_coro::http
