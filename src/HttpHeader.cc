/**
 * @file HttpHeader.cc
 * @brief Implementation of HttpHeader
 */
#include <cctype>
#include <nitro_coro/http/HttpHeader.h>

namespace nitro_coro::http
{

HttpHeader::HttpHeader(std::string name, std::string value)
    : name_(toLower(name)), canonicalName_(toCanonical(name)), value_(trim(std::move(value)))
{
}

bool HttpHeader::nameEquals(std::string_view name) const
{
    if (name.size() != name_.size())
        return false;

    for (size_t i = 0; i < name.size(); ++i)
    {
        if (std::tolower(static_cast<unsigned char>(name[i])) != name_[i])
            return false;
    }
    return true;
}

std::string HttpHeader::serialize() const
{
    return canonicalName_ + ": " + value_ + "\r\n";
}

std::string HttpHeader::toLower(const std::string & str)
{
    std::string result = str;
    std::transform(result.begin(), result.end(), result.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    return result;
}

std::string HttpHeader::toCanonical(const std::string & str)
{
    std::string result = str;
    bool capitalizeNext = true;

    for (char & c : result)
    {
        if (capitalizeNext && std::isalpha(static_cast<unsigned char>(c)))
        {
            c = std::toupper(static_cast<unsigned char>(c));
            capitalizeNext = false;
        }
        else if (c == '-')
        {
            capitalizeNext = true;
        }
        else
        {
            c = std::tolower(static_cast<unsigned char>(c));
        }
    }
    return result;
}

std::string HttpHeader::trim(const std::string & str)
{
    auto start = str.begin();
    while (start != str.end() && std::isspace(static_cast<unsigned char>(*start)))
        ++start;

    auto end = str.end();
    while (end != start && std::isspace(static_cast<unsigned char>(*(end - 1))))
        --end;

    return std::string(start, end);
}

} // namespace nitro_coro::http
