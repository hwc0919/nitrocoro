/**
 * @file HttpRequestAccessor.h
 * @brief Request-specific data accessor
 */
#pragma once

#include <nitro_coro/http/HttpDataAccessor.h>
#include <nitro_coro/http/HttpMessage.h>

namespace nitro_coro::http
{

template <typename Derived>
class HttpRequestAccessor : public HttpDataAccessor<Derived, HttpRequest>
{
public:
    const std::string & method() const { return this->data().method; }
    const std::string & path() const { return this->data().path; }
    const std::string & version() const { return this->data().version; }
    const std::map<std::string, std::string> & queries() const { return this->data().queries; }

    std::string_view query(const std::string & name) const
    {
        auto it = this->data().queries.find(name);
        return it != this->data().queries.end() ? std::string_view(it->second) : std::string_view();
    }
};

} // namespace nitro_coro::http
