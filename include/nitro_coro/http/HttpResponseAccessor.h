/**
 * @file HttpResponseAccessor.h
 * @brief Response-specific data accessor
 */
#pragma once

#include <nitro_coro/http/HttpDataAccessor.h>
#include <nitro_coro/http/HttpMessage.h>

namespace nitro_coro::http
{

template <typename Derived>
class HttpResponseAccessor : public HttpDataAccessor<Derived, HttpResponse>
{
public:
    int statusCode() const { return this->data().statusCode; }
    const std::string & statusReason() const { return this->data().statusReason; }
    const std::string & version() const { return this->data().version; }
};

} // namespace nitro_coro::http
