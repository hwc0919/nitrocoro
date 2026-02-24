/**
 * @file HttpDataAccessor.h
 * @brief CRTP base class for HTTP data access
 */
#pragma once
#include <nitro_coro/http/HttpHeader.h>
#include <nitro_coro/http/HttpTypes.h>

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
    const std::map<std::string, HttpHeader, std::less<>> & headers() const { return data().headers; }
    const std::map<std::string, std::string, std::less<>> & cookies() const { return data().cookies; }

    const std::string & getHeader(std::string_view name) const
    {
        static const std::string emptyValue{};
        std::string lowerName{ name };
        std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(),
                       [](unsigned char c) { return std::tolower(c); });
        auto it = data().headers.find(lowerName);
        return it != data().headers.end() ? it->second.value() : emptyValue;
    }

    const std::string & getHeader(HttpHeader::NameCode code) const
    {
        static const std::string emptyValue{};
        auto it = data().headers.find(HttpHeader::codeToName(code));
        return it != data().headers.end() ? it->second.value() : emptyValue;
    }

    const std::string & getCookie(std::string_view name) const
    {
        static const std::string emptyValue{};
        auto it = data().cookies.find(name);
        return it != data().cookies.end() ? it->second : emptyValue;
    }
};

template <typename Derived>
class HttpRequestAccessor : public HttpDataAccessor<Derived, HttpRequest>
{
public:
    const std::string & method() const { return this->data().method; }
    const std::string & path() const { return this->data().path; }
    Version version() const { return this->data().version; }
    const std::map<std::string, std::string, std::less<>> & queries() const { return this->data().queries; }

    const std::string & getQuery(std::string_view name) const
    {
        static const std::string emptyValue{};
        auto it = this->data().queries.find(name);
        return it != this->data().queries.end() ? it->second : emptyValue;
    }
};

template <typename Derived>
class HttpResponseAccessor : public HttpDataAccessor<Derived, HttpResponse>
{
public:
    StatusCode statusCode() const { return this->data().statusCode; }
    const std::string & statusReason() const { return this->data().statusReason; }
    Version version() const { return this->data().version; }
};

} // namespace nitro_coro::http
