/**
 * @file HttpParser.cc
 * @brief HTTP parser implementations
 */
#include <nitro_coro/http/HttpHeader.h>
#include <nitro_coro/http/HttpParser.h>

namespace nitro_coro::http
{

static Version parseHttpVersion(std::string_view versionStr)
{
    if (versionStr == "HTTP/1.0")
        return Version::kHttp10;
    if (versionStr == "HTTP/1.1")
        return Version::kHttp11;
    return Version::kUnknown;
}

static StatusCode parseStatusCode(int code)
{
    return static_cast<StatusCode>(code);
}

// ============================================================================
// HttpParser<HttpRequest> Implementation
// ============================================================================

bool HttpParser<HttpRequest>::parseLine(std::string_view line)
{
    if (data_.method.empty())
    {
        parseRequestLine(line);
    }
    else if (!line.empty())
    {
        parseHeader(line);
    }
    else
    {
        auto it = data_.headers.find(HttpHeader::Name::ContentLength_L);
        if (it != data_.headers.end())
        {
            contentLength_ = std::stoul(it->second.value());
            transferMode_ = TransferMode::ContentLength;
        }

        it = data_.headers.find(HttpHeader::Name::TransferEncoding_L);
        if (it != data_.headers.end() && it->second.value().find("chunked") != std::string::npos)
        {
            transferMode_ = TransferMode::Chunked;
        }

        headerComplete_ = true;
    }
    return headerComplete_;
}

void HttpParser<HttpRequest>::parseRequestLine(std::string_view line)
{
    size_t pos1 = line.find(' ');
    size_t pos2 = line.find(' ', pos1 + 1);

    data_.method = line.substr(0, pos1);
    data_.fullPath = line.substr(pos1 + 1, pos2 - pos1 - 1);
    data_.version = parseHttpVersion(line.substr(pos2 + 1));

    size_t qpos = data_.fullPath.find('?');
    if (qpos != std::string::npos)
    {
        data_.path = std::string_view(data_.fullPath).substr(0, qpos);
        data_.query = std::string_view(data_.fullPath).substr(qpos + 1);
        parseQueryString(data_.query);
    }
    else
    {
        data_.path = data_.fullPath;
        data_.query = std::string_view();
    }
}

void HttpParser<HttpRequest>::parseHeader(std::string_view line)
{
    size_t pos = line.find(':');
    if (pos == std::string::npos)
        return;

    std::string name(line.substr(0, pos));
    std::string value(line.substr(pos + 1));

    HttpHeader header(std::move(name), std::move(value));

    if (header.name() == "cookie")
    {
        parseCookies(header.value());
    }
    else
    {
        data_.headers.emplace(header.name(), std::move(header));
    }
}

void HttpParser<HttpRequest>::parseQueryString(std::string_view queryStr)
{
    // TODO: url decode
    // TODO: multi-value
    size_t start = 0;
    while (start < queryStr.size())
    {
        size_t ampPos = queryStr.find('&', start);
        size_t end = (ampPos == std::string_view::npos) ? queryStr.size() : ampPos;

        std::string_view pair = queryStr.substr(start, end - start);
        size_t eqPos = pair.find('=');
        if (eqPos != std::string_view::npos)
        {
            std::string key = std::string(pair.substr(0, eqPos));
            std::string value = std::string(pair.substr(eqPos + 1));
            data_.queries.emplace(std::move(key), std::move(value));
        }

        if (ampPos == std::string_view::npos)
            break;
        start = ampPos + 1;
    }
}

void HttpParser<HttpRequest>::parseCookies(const std::string & cookieHeader)
{
    // TODO: parse cookies correctly
    std::string_view cookies(cookieHeader);
    size_t start = 0;
    while (start < cookies.size())
    {
        while (start < cookies.size() && cookies[start] == ' ')
            ++start;

        size_t semiPos = cookies.find(';', start);
        size_t end = (semiPos == std::string_view::npos) ? cookies.size() : semiPos;

        std::string_view pair = cookies.substr(start, end - start);
        size_t eqPos = pair.find('=');
        if (eqPos != std::string_view::npos)
        {
            data_.cookies[std::string(pair.substr(0, eqPos))] = std::string(pair.substr(eqPos + 1));
        }

        if (semiPos == std::string_view::npos)
            break;
        start = semiPos + 1;
    }
}

// ============================================================================
// HttpParser<HttpResponse> Implementation
// ============================================================================

bool HttpParser<HttpResponse>::parseLine(std::string_view line)
{
    if (data_.statusCode == StatusCode::kUnknown)
    {
        parseStatusLine(line);
    }
    else if (!line.empty())
    {
        parseHeader(line);
    }
    else
    {
        auto it = data_.headers.find(HttpHeader::Name::ContentLength_L);
        if (it != data_.headers.end())
        {
            contentLength_ = std::stoul(it->second.value());
            transferMode_ = TransferMode::ContentLength;
        }

        it = data_.headers.find(HttpHeader::Name::TransferEncoding_L);
        if (it != data_.headers.end() && it->second.value().find("chunked") != std::string::npos)
        {
            transferMode_ = TransferMode::Chunked;
        }

        headerComplete_ = true;
    }
    return headerComplete_;
}

void HttpParser<HttpResponse>::parseStatusLine(std::string_view line)
{
    size_t sp1 = line.find(' ');
    size_t sp2 = line.find(' ', sp1 + 1);

    data_.version = parseHttpVersion(line.substr(0, sp1));
    data_.statusCode = parseStatusCode(std::stoi(std::string(line.substr(sp1 + 1, sp2 - sp1 - 1))));
    data_.statusReason = line.substr(sp2 + 1);
}

void HttpParser<HttpResponse>::parseHeader(std::string_view line)
{
    size_t pos = line.find(':');
    if (pos == std::string::npos)
        return;

    std::string name(line.substr(0, pos));
    std::string value(line.substr(pos + 1));

    HttpHeader header(std::move(name), std::move(value));

    if (header.name() == "set-cookie")
    {
        parseCookies(header.value());
    }
    else
    {
        data_.headers.emplace(header.name(), std::move(header));
    }
}

void HttpParser<HttpResponse>::parseCookies(const std::string & cookieHeader)
{
    size_t eqPos = cookieHeader.find('=');
    if (eqPos != std::string::npos)
    {
        size_t endPos = cookieHeader.find(';', eqPos);
        std::string name = cookieHeader.substr(0, eqPos);
        std::string value = (endPos != std::string::npos)
                                ? cookieHeader.substr(eqPos + 1, endPos - eqPos - 1)
                                : cookieHeader.substr(eqPos + 1);
        data_.cookies.insert_or_assign(std::move(name), std::move(value));
    }
}

} // namespace nitro_coro::http
