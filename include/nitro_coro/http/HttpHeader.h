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
    enum class NameCode
    {
        Unknown,

        // General
        CacheControl,
        Connection,
        Date,
        TransferEncoding,
        Upgrade,

        // Request
        Accept,
        AcceptEncoding,
        AcceptLanguage,
        Authorization,
        Host,
        IfModifiedSince,
        IfNoneMatch,
        Referer,
        UserAgent,

        // Response
        AcceptRanges,
        Age,
        ETag,
        Location,
        RetryAfter,
        Server,
        Vary,
        WwwAuthenticate,

        // Entity
        Allow,
        ContentEncoding,
        ContentLanguage,
        ContentLength,
        ContentRange,
        ContentType,
        Expires,
        LastModified,

        // Cookie
        Cookie,
        SetCookie,

        // CORS
        AccessControlAllowOrigin,
        AccessControlAllowMethods,
        AccessControlAllowHeaders,
        AccessControlAllowCredentials,
        Origin,

        // Custom
        XForwardedFor,
        XForwardedProto,
        XRealIp,
    };

    HttpHeader(const std::string & name, std::string value);
    HttpHeader(NameCode name, std::string value);

    const std::string & name() const { return name_; }
    const std::string & canonicalName() const { return canonicalName_; }
    const std::string & value() const { return value_; }
    NameCode nameCode() const { return nameCode_; }

    bool nameEquals(std::string_view name) const;
    bool nameEqualsLower(std::string_view lowerName) const { return name_ == lowerName; }
    bool nameCodeEquals(NameCode code) const { return nameCode_ == code; }
    std::string serialize() const;

    static const std::pair<std::string_view, std::string_view> & codeToNames(NameCode code);
    static std::string_view codeToName(NameCode code);
    static std::string_view codeToCanonicalName(NameCode code);
    static NameCode nameToCode(const std::string & lowerName);

    static std::string toLower(const std::string & str);
    static std::string toCanonical(const std::string & str);

private:
    std::string name_;
    std::string canonicalName_;
    std::string value_;
    NameCode nameCode_;
};

} // namespace nitro_coro::http
