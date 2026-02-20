/**
 * @file HttpDataAccessor.h
 * @brief CRTP base class for HTTP data access
 */
#pragma once
#include <nitro_coro/http/HttpHeader.h>

#include <algorithm>
#include <map>
#include <string>
#include <string_view>

namespace nitro_coro::http
{

struct HttpRequest;
struct HttpResponse;

template <typename Derived, typename DataType>
class HttpDataAccessor
{
protected:
    const DataType & data() const { return static_cast<const Derived *>(this)->getData(); }

public:
    const std::map<std::string, HttpHeader> & headers() const { return data().headers; }
    const std::map<std::string, std::string> & cookies() const { return data().cookies; }

    std::string_view getHeader(std::string_view name) const
    {
        std::string lowerName{ name };
        std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(),
                       [](unsigned char c) { return std::tolower(c); });
        auto it = data().headers.find(lowerName);
        return it != data().headers.end() ? std::string_view(it->second.value()) : std::string_view();
    }

    std::string_view getHeader(HttpHeader::NameCode code) const
    {
        auto name = HttpHeader::codeToName(code);
        auto it = data().headers.find(std::string(name));
        return it != data().headers.end() ? std::string_view(it->second.value()) : std::string_view();
    }

    std::string_view getCookie(const std::string & name) const
    {
        auto it = data().cookies.find(name);
        return it != data().cookies.end() ? std::string_view(it->second) : std::string_view();
    }
};

template <typename Derived>
class HttpRequestAccessor : public HttpDataAccessor<Derived, HttpRequest>
{
public:
    const std::string & method() const { return this->data().method; }
    const std::string & path() const { return this->data().path; }
    const std::string & version() const { return this->data().version; }
    const std::map<std::string, std::string> & queries() const { return this->data().queries; }

    std::string_view getQuery(const std::string & name) const
    {
        auto it = this->data().queries.find(name);
        return it != this->data().queries.end() ? std::string_view(it->second) : std::string_view();
    }
};

template <typename Derived>
class HttpResponseAccessor : public HttpDataAccessor<Derived, HttpResponse>
{
public:
    int statusCode() const { return this->data().statusCode; }
    const std::string & statusReason() const { return this->data().statusReason; }
    const std::string & version() const { return this->data().version; }
};

} // namespace nitro_coro::http
