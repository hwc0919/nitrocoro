/**
 * @file HttpParser.cc
 * @brief HTTP parser implementations
 */
#include <nitro_coro/http/HttpHeader.h>
#include <nitro_coro/http/HttpParser.h>
#include <sstream>

namespace nitro_coro::http
{

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
        static const std::string contentLengthKey{ HttpHeader::Name::ContentLength_L };
        auto it = data_.headers.find(contentLengthKey);
        if (it != data_.headers.end())
        {
            contentLength_ = std::stoul(it->second.value());
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
    std::string fullPath(line.substr(pos1 + 1, pos2 - pos1 - 1));
    data_.version = line.substr(pos2 + 1);

    size_t qpos = fullPath.find('?');
    if (qpos != std::string::npos)
    {
        data_.path = fullPath.substr(0, qpos);
        parseQueryString();
    }
    else
    {
        data_.path = fullPath;
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

void HttpParser<HttpRequest>::parseQueryString()
{
    size_t qpos = data_.path.find('?');
    if (qpos == std::string::npos)
        return;

    std::string queryStr = data_.path.substr(qpos + 1);
    data_.path = data_.path.substr(0, qpos);

    std::istringstream iss(queryStr);
    std::string pair;
    while (std::getline(iss, pair, '&'))
    {
        size_t eqPos = pair.find('=');
        if (eqPos != std::string::npos)
        {
            data_.queries[pair.substr(0, eqPos)] = pair.substr(eqPos + 1);
        }
    }
}

void HttpParser<HttpRequest>::parseCookies(const std::string & cookieHeader)
{
    // TODO: Implement cookie parsing
}

// ============================================================================
// HttpParser<HttpResponse> Implementation
// ============================================================================

bool HttpParser<HttpResponse>::parseLine(std::string_view line)
{
    if (data_.statusCode == 0)
    {
        parseStatusLine(line);
    }
    else if (!line.empty())
    {
        parseHeader(line);
    }
    else
    {
        static const std::string contentLengthKey{ HttpHeader::Name::ContentLength_L };
        auto it = data_.headers.find(contentLengthKey);
        if (it != data_.headers.end())
        {
            contentLength_ = std::stoul(it->second.value());
        }
        headerComplete_ = true;
    }
    return headerComplete_;
}

void HttpParser<HttpResponse>::parseStatusLine(std::string_view line)
{
    size_t sp1 = line.find(' ');
    size_t sp2 = line.find(' ', sp1 + 1);

    data_.version = line.substr(0, sp1);
    data_.statusCode = std::stoi(std::string(line.substr(sp1 + 1, sp2 - sp1 - 1)));
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
        size_t endPos = cookieHeader.find(';');
        std::string cookieName = cookieHeader.substr(0, eqPos);
        std::string cookieValue = (endPos != std::string::npos)
                                      ? cookieHeader.substr(eqPos + 1, endPos - eqPos - 1)
                                      : cookieHeader.substr(eqPos + 1);
        data_.cookies[cookieName] = cookieValue;
    }
}

} // namespace nitro_coro::http
