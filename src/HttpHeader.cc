/**
 * @file HttpHeader.cc
 * @brief Implementation of HttpHeader
 */
#include <cctype>
#include <nitro_coro/http/HttpHeader.h>
#include <unordered_map>

namespace nitro_coro::http
{

HttpHeader::HttpHeader(const std::string & name, std::string value)
    : name_(toLower(name))
    , canonicalName_(toCanonical(name))
    , value_(std::move(value))
    , nameCode_(nameToCode(name_))
{
}

HttpHeader::HttpHeader(NameCode name, std::string value)
{
    auto & [lower, canonical] = codeToNames(name);
    name_ = lower;
    canonicalName_ = canonical;
    value_ = std::move(value);
    nameCode_ = name;
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

const std::pair<std::string_view, std::string_view> & HttpHeader::codeToNames(NameCode code)
{
    static constexpr std::pair<std::string_view, std::string_view> pairs[] = {
        { "", "" },
        { "cache-control", "Cache-Control" },
        { "connection", "Connection" },
        { "date", "Date" },
        { "transfer-encoding", "Transfer-Encoding" },
        { "upgrade", "Upgrade" },
        { "accept", "Accept" },
        { "accept-encoding", "Accept-Encoding" },
        { "accept-language", "Accept-Language" },
        { "authorization", "Authorization" },
        { "host", "Host" },
        { "if-modified-since", "If-Modified-Since" },
        { "if-none-match", "If-None-Match" },
        { "referer", "Referer" },
        { "user-agent", "User-Agent" },
        { "accept-ranges", "Accept-Ranges" },
        { "age", "Age" },
        { "etag", "ETag" },
        { "location", "Location" },
        { "retry-after", "Retry-After" },
        { "server", "Server" },
        { "vary", "Vary" },
        { "www-authenticate", "WWW-Authenticate" },
        { "allow", "Allow" },
        { "content-encoding", "Content-Encoding" },
        { "content-language", "Content-Language" },
        { "content-length", "Content-Length" },
        { "content-range", "Content-Range" },
        { "content-type", "Content-Type" },
        { "expires", "Expires" },
        { "last-modified", "Last-Modified" },
        { "cookie", "Cookie" },
        { "set-cookie", "Set-Cookie" },
        { "access-control-allow-origin", "Access-Control-Allow-Origin" },
        { "access-control-allow-methods", "Access-Control-Allow-Methods" },
        { "access-control-allow-headers", "Access-Control-Allow-Headers" },
        { "access-control-allow-credentials", "Access-Control-Allow-Credentials" },
        { "origin", "Origin" },
        { "x-forwarded-for", "X-Forwarded-For" },
        { "x-forwarded-proto", "X-Forwarded-Proto" },
        { "x-real-ip", "X-Real-Ip" },
    };
    return pairs[static_cast<size_t>(code)];
}

std::string_view HttpHeader::codeToName(NameCode code)
{
    return codeToNames(code).first;
}

std::string_view HttpHeader::codeToCanonicalName(NameCode code)
{
    return codeToNames(code).second;
}

HttpHeader::NameCode HttpHeader::nameToCode(const std::string & lowerName)
{
    static const std::unordered_map<std::string_view, NameCode> nameMap = {
        { "cache-control", NameCode::CacheControl },
        { "connection", NameCode::Connection },
        { "date", NameCode::Date },
        { "transfer-encoding", NameCode::TransferEncoding },
        { "upgrade", NameCode::Upgrade },
        { "accept", NameCode::Accept },
        { "accept-encoding", NameCode::AcceptEncoding },
        { "accept-language", NameCode::AcceptLanguage },
        { "authorization", NameCode::Authorization },
        { "host", NameCode::Host },
        { "if-modified-since", NameCode::IfModifiedSince },
        { "if-none-match", NameCode::IfNoneMatch },
        { "referer", NameCode::Referer },
        { "user-agent", NameCode::UserAgent },
        { "accept-ranges", NameCode::AcceptRanges },
        { "age", NameCode::Age },
        { "etag", NameCode::ETag },
        { "location", NameCode::Location },
        { "retry-after", NameCode::RetryAfter },
        { "server", NameCode::Server },
        { "vary", NameCode::Vary },
        { "www-authenticate", NameCode::WwwAuthenticate },
        { "allow", NameCode::Allow },
        { "content-encoding", NameCode::ContentEncoding },
        { "content-language", NameCode::ContentLanguage },
        { "content-length", NameCode::ContentLength },
        { "content-range", NameCode::ContentRange },
        { "content-type", NameCode::ContentType },
        { "expires", NameCode::Expires },
        { "last-modified", NameCode::LastModified },
        { "cookie", NameCode::Cookie },
        { "set-cookie", NameCode::SetCookie },
        { "access-control-allow-origin", NameCode::AccessControlAllowOrigin },
        { "access-control-allow-methods", NameCode::AccessControlAllowMethods },
        { "access-control-allow-headers", NameCode::AccessControlAllowHeaders },
        { "access-control-allow-credentials", NameCode::AccessControlAllowCredentials },
        { "origin", NameCode::Origin },
        { "x-forwarded-for", NameCode::XForwardedFor },
        { "x-forwarded-proto", NameCode::XForwardedProto },
        { "x-real-ip", NameCode::XRealIp },
    };

    auto it = nameMap.find(lowerName);
    return it != nameMap.end() ? it->second : NameCode::Unknown;
}

} // namespace nitro_coro::http
