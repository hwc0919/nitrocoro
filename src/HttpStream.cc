/**
 * @file HttpStream.cc
 * @brief HTTP stream implementations
 */
#include <algorithm>
#include <cstring>
#include <nitro_coro/http/HttpStream.h>
#include <sstream>

namespace nitro_coro::http
{

// ============================================================================
// HttpIncomingStream<HttpRequest> Implementation
// ============================================================================

int HttpIncomingStream<HttpRequest>::parse(const char * data, size_t len)
{
    buffer_.append(data, len);

    while (!headerComplete_)
    {
        size_t pos = buffer_.find("\r\n");
        if (pos == std::string::npos)
            return len;

        std::string line = buffer_.substr(0, pos);
        buffer_.erase(0, pos + 2);

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
            // End of headers
            static const std::string contentLengthKey{ HttpHeader::codeToName(HttpHeader::NameCode::ContentLength) };
            auto it = data_.headers.find(contentLengthKey);
            if (it != data_.headers.end())
            {
                contentLength_ = std::stoul(it->second.value());
            }
            headerComplete_ = true;
            if (contentLength_ == 0)
                complete_ = true;
        }
    }

    return len;
}

void HttpIncomingStream<HttpRequest>::parseRequestLine(std::string_view line)
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

void HttpIncomingStream<HttpRequest>::parseHeader(std::string_view line)
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

void HttpIncomingStream<HttpRequest>::parseQueryString()
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

void HttpIncomingStream<HttpRequest>::parseCookies(const std::string & cookieHeader)
{
    // TODO: Implement cookie parsing
}

std::string_view HttpIncomingStream<HttpRequest>::header(HttpHeader::NameCode code) const
{
    auto name = HttpHeader::codeToName(code);
    auto it = data_.headers.find(std::string(name));
    return it != data_.headers.end() ? std::string_view(it->second.value()) : std::string_view();
}

std::string_view HttpIncomingStream<HttpRequest>::header(const std::string & name) const
{
    std::string lowerName = name;
    std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(),
                   [](unsigned char c) { return std::tolower(c); });

    auto it = data_.headers.find(lowerName);
    return it != data_.headers.end() ? std::string_view(it->second.value()) : std::string_view();
}

std::string_view HttpIncomingStream<HttpRequest>::query(const std::string & name) const
{
    auto it = data_.queries.find(name);
    return it != data_.queries.end() ? std::string_view(it->second) : std::string_view();
}

std::string_view HttpIncomingStream<HttpRequest>::cookie(const std::string & name) const
{
    auto it = data_.cookies.find(name);
    return it != data_.cookies.end() ? std::string_view(it->second) : std::string_view();
}

Task<std::string_view> HttpIncomingStream<HttpRequest>::read(size_t maxSize)
{
    if (bodyBytesRead_ >= contentLength_)
        co_return std::string_view();

    size_t oldSize = buffer_.size();
    if (bodyBytesRead_ == 0 && oldSize > 0)
    {
        size_t n = std::min(oldSize, contentLength_);
        bodyBytesRead_ += n;
        if (bodyBytesRead_ >= contentLength_)
            complete_ = true;
        co_return std::string_view(buffer_.data(), n);
    }

    size_t toRead = std::min(maxSize, contentLength_ - bodyBytesRead_);

    buffer_.resize(oldSize + toRead);
    size_t n = co_await conn_->read(buffer_.data() + oldSize, toRead);
    buffer_.resize(oldSize + n);

    bodyBytesRead_ += n;
    if (bodyBytesRead_ >= contentLength_)
        complete_ = true;

    co_return std::string_view(buffer_.data() + oldSize, n);
}

Task<size_t> HttpIncomingStream<HttpRequest>::readTo(char * buf, size_t len)
{
    if (!buffer_.empty())
    {
        size_t toRead = std::min(len, buffer_.size());
        std::memcpy(buf, buffer_.data(), toRead);
        buffer_.erase(0, toRead);
        bodyBytesRead_ += toRead;

        if (bodyBytesRead_ >= contentLength_)
            complete_ = true;

        co_return toRead;
    }

    if (conn_ && bodyBytesRead_ < contentLength_)
    {
        size_t remaining = contentLength_ - bodyBytesRead_;
        size_t toRead = std::min(len, remaining);
        size_t n = co_await conn_->read(buf, toRead);

        bodyBytesRead_ += n;
        if (bodyBytesRead_ >= contentLength_)
            complete_ = true;

        co_return n;
    }

    co_return 0;
}

Task<std::string_view> HttpIncomingStream<HttpRequest>::readAll()
{
    if (bodyBytesRead_ >= contentLength_)
        co_return std::string_view(buffer_);

    size_t oldSize = buffer_.size();

    while (bodyBytesRead_ < contentLength_)
    {
        constexpr size_t CHUNK_SIZE = 4096;
        size_t currentSize = buffer_.size();
        size_t toRead = std::min(CHUNK_SIZE, contentLength_ - bodyBytesRead_);

        buffer_.reserve(currentSize + toRead);
        size_t n = co_await conn_->read(buffer_.data() + currentSize, toRead);
        buffer_.resize(currentSize + n);

        bodyBytesRead_ += n;
    }

    if (bodyBytesRead_ >= contentLength_)
        complete_ = true;

    co_return std::string_view(buffer_.data() + oldSize, buffer_.size() - oldSize);
}

// ============================================================================
// HttpIncomingStream<HttpResponse> Implementation
// ============================================================================

int HttpIncomingStream<HttpResponse>::parse(const char * data, size_t len)
{
    buffer_.append(data, len);

    while (!headerComplete_)
    {
        size_t pos = buffer_.find("\r\n");
        if (pos == std::string::npos)
            return len;

        std::string line = buffer_.substr(0, pos);
        buffer_.erase(0, pos + 2);

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
            // End of headers
            static const std::string contentLengthKey{ HttpHeader::codeToName(HttpHeader::NameCode::ContentLength) };
            auto it = data_.headers.find(contentLengthKey);
            if (it != data_.headers.end())
            {
                contentLength_ = std::stoul(it->second.value());
            }
            headerComplete_ = true;
            if (contentLength_ == 0)
                complete_ = true;
        }
    }

    return len;
}

void HttpIncomingStream<HttpResponse>::parseStatusLine(std::string_view line)
{
    // Parse "HTTP/1.1 200 OK"
    size_t sp1 = line.find(' ');
    size_t sp2 = line.find(' ', sp1 + 1);

    data_.version = line.substr(0, sp1);
    data_.statusCode = std::stoi(std::string(line.substr(sp1 + 1, sp2 - sp1 - 1)));
    data_.statusReason = line.substr(sp2 + 1);
}

void HttpIncomingStream<HttpResponse>::parseHeader(std::string_view line)
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

void HttpIncomingStream<HttpResponse>::parseCookies(const std::string & cookieHeader)
{
    // TODO: Parse Set-Cookie header properly
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

std::string_view HttpIncomingStream<HttpResponse>::header(const std::string & name) const
{
    std::string lowerName = name;
    std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(),
                   [](unsigned char c) { return std::tolower(c); });

    auto it = data_.headers.find(lowerName);
    return it != data_.headers.end() ? std::string_view(it->second.value()) : std::string_view();
}

std::string_view HttpIncomingStream<HttpResponse>::cookie(const std::string & name) const
{
    auto it = data_.cookies.find(name);
    return it != data_.cookies.end() ? std::string_view(it->second) : std::string_view();
}

Task<std::string_view> HttpIncomingStream<HttpResponse>::read(size_t maxSize)
{
    if (bodyBytesRead_ >= contentLength_)
        co_return std::string_view();

    size_t oldSize = buffer_.size();
    if (bodyBytesRead_ == 0 && oldSize > 0)
    {
        size_t n = std::min(oldSize, contentLength_);
        bodyBytesRead_ += n;
        if (bodyBytesRead_ >= contentLength_)
            complete_ = true;
        co_return std::string_view(buffer_.data(), n);
    }

    size_t toRead = std::min(maxSize, contentLength_ - bodyBytesRead_);

    buffer_.resize(oldSize + toRead);
    size_t n = co_await conn_->read(buffer_.data() + oldSize, toRead);
    buffer_.resize(oldSize + n);

    bodyBytesRead_ += n;
    if (bodyBytesRead_ >= contentLength_)
        complete_ = true;

    co_return std::string_view(buffer_.data() + oldSize, n);
}

Task<size_t> HttpIncomingStream<HttpResponse>::readTo(char * buf, size_t len)
{
    if (!buffer_.empty())
    {
        size_t toRead = std::min(len, buffer_.size());
        std::memcpy(buf, buffer_.data(), toRead);
        buffer_.erase(0, toRead);
        bodyBytesRead_ += toRead;

        if (bodyBytesRead_ >= contentLength_)
            complete_ = true;

        co_return toRead;
    }

    if (conn_ && bodyBytesRead_ < contentLength_)
    {
        size_t remaining = contentLength_ - bodyBytesRead_;
        size_t toRead = std::min(len, remaining);
        size_t n = co_await conn_->read(buf, toRead);

        bodyBytesRead_ += n;
        if (bodyBytesRead_ >= contentLength_)
            complete_ = true;

        co_return n;
    }

    co_return 0;
}

Task<std::string_view> HttpIncomingStream<HttpResponse>::readAll()
{
    if (bodyBytesRead_ >= contentLength_)
        co_return std::string_view(buffer_);

    size_t oldSize = buffer_.size();

    while (bodyBytesRead_ < contentLength_)
    {
        constexpr size_t CHUNK_SIZE = 4096;
        size_t currentSize = buffer_.size();
        size_t toRead = std::min(CHUNK_SIZE, contentLength_ - bodyBytesRead_);

        buffer_.reserve(currentSize + toRead);
        size_t n = co_await conn_->read(buffer_.data() + currentSize, toRead);
        buffer_.resize(currentSize + n);

        bodyBytesRead_ += n;
    }

    if (bodyBytesRead_ >= contentLength_)
        complete_ = true;

    co_return std::string_view(buffer_.data() + oldSize, buffer_.size() - oldSize);
}

// ============================================================================
// HttpOutgoingStream<HttpRequest> Implementation
// ============================================================================

void HttpOutgoingStream<HttpRequest>::setHeader(const std::string & name, const std::string & value)
{
    HttpHeader header(name, value);
    data_.headers.insert_or_assign(header.name(), std::move(header));
}

void HttpOutgoingStream<HttpRequest>::setCookie(const std::string & name, const std::string & value)
{
    data_.cookies[name] = value;
}

Task<> HttpOutgoingStream<HttpRequest>::writeHeaders()
{
    if (headersSent_)
        co_return;

    std::ostringstream oss;
    oss << data_.method << " " << data_.path << " " << data_.version << "\r\n";

    for (const auto & [name, header] : data_.headers)
    {
        oss << header.name() << ": " << header.value() << "\r\n";
    }

    for (const auto & [name, value] : data_.cookies)
    {
        oss << "Cookie: " << name << "=" << value << "\r\n";
    }

    oss << "\r\n";

    std::string headers = oss.str();
    co_await conn_->write(headers.c_str(), headers.size());
    headersSent_ = true;
}

Task<> HttpOutgoingStream<HttpRequest>::write(const char * data, size_t len)
{
    co_await writeHeaders();
    co_await conn_->write(data, len);
}

Task<> HttpOutgoingStream<HttpRequest>::write(std::string_view data)
{
    co_await write(data.data(), data.size());
}

Task<> HttpOutgoingStream<HttpRequest>::end()
{
    co_await writeHeaders();
}

Task<> HttpOutgoingStream<HttpRequest>::end(std::string_view data)
{
    co_await write(data);
}

// ============================================================================
// HttpOutgoingStream<HttpResponse> Implementation
// ============================================================================

void HttpOutgoingStream<HttpResponse>::setStatus(int code, const std::string & reason)
{
    data_.statusCode = code;
    data_.statusReason = reason.empty() ? getDefaultReason(code) : reason;
}

void HttpOutgoingStream<HttpResponse>::setHeader(HttpHeader header)
{
    data_.headers.insert_or_assign(header.name(), std::move(header));
}

void HttpOutgoingStream<HttpResponse>::setHeader(const std::string & name, const std::string & value)
{
    HttpHeader header(name, value);
    data_.headers.insert_or_assign(header.name(), std::move(header));
}

void HttpOutgoingStream<HttpResponse>::setCookie(const std::string & name, const std::string & value)
{
    data_.cookies[name] = value;
}

Task<> HttpOutgoingStream<HttpResponse>::writeHeaders()
{
    if (headersSent_)
        co_return;

    std::ostringstream oss;
    oss << data_.version << " " << data_.statusCode << " " << data_.statusReason << "\r\n";

    for (const auto & [name, header] : data_.headers)
    {
        oss << header.name() << ": " << header.value() << "\r\n";
    }

    for (const auto & [name, value] : data_.cookies)
    {
        oss << "Set-Cookie: " << name << "=" << value << "\r\n";
    }

    oss << "\r\n";

    std::string headers = oss.str();
    co_await conn_->write(headers.c_str(), headers.size());
    headersSent_ = true;
}

Task<> HttpOutgoingStream<HttpResponse>::write(const char * data, size_t len)
{
    co_await writeHeaders();
    co_await conn_->write(data, len);
}

Task<> HttpOutgoingStream<HttpResponse>::write(std::string_view data)
{
    co_await write(data.data(), data.size());
}

Task<> HttpOutgoingStream<HttpResponse>::end()
{
    co_await writeHeaders();
}

Task<> HttpOutgoingStream<HttpResponse>::end(std::string_view data)
{
    co_await write(data);
}

const char * HttpOutgoingStream<HttpResponse>::getDefaultReason(int code)
{
    switch (code)
    {
        case 200:
            return "OK";
        case 201:
            return "Created";
        case 204:
            return "No Content";
        case 301:
            return "Moved Permanently";
        case 302:
            return "Found";
        case 304:
            return "Not Modified";
        case 400:
            return "Bad Request";
        case 401:
            return "Unauthorized";
        case 403:
            return "Forbidden";
        case 404:
            return "Not Found";
        case 500:
            return "Internal Server Error";
        case 502:
            return "Bad Gateway";
        case 503:
            return "Service Unavailable";
        default:
            return "";
    }
}

} // namespace nitro_coro::http
