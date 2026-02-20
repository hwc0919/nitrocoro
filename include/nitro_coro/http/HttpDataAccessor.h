/**
 * @file HttpDataAccessor.h
 * @brief CRTP base class for HTTP data access
 */
#pragma once

#include <algorithm>
#include <map>
#include <nitro_coro/http/HttpHeader.h>
#include <string>
#include <string_view>

namespace nitro_coro::http
{

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

} // namespace nitro_coro::http
