/**
 * @file HttpHeader.h
 * @brief HTTP header representation
 */
#pragma once

#include <algorithm>
#include <string>
#include <string_view>

namespace nitro_coro::http
{

class HttpHeader
{
public:
    HttpHeader(std::string name, std::string value);

    const std::string & name() const { return name_; }
    const std::string & canonicalName() const { return canonicalName_; }
    const std::string & value() const { return value_; }

    bool nameEquals(std::string_view name) const;
    std::string serialize() const;

    bool operator==(const HttpHeader & other) const
    {
        return name_ == other.name_;
    }

private:
    std::string name_;
    std::string canonicalName_;
    std::string value_;

    static std::string toLower(const std::string & str);
    static std::string toCanonical(const std::string & str);
};

} // namespace nitro_coro::http
