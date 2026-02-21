/**
 * @file HttpIncomingStream.cc
 * @brief HTTP incoming stream implementations
 */
#include <nitro_coro/http/HttpMessage.h>
#include <nitro_coro/http/stream/HttpIncomingStream.h>

#include <algorithm>
#include <cstring>
#include <sstream>

namespace nitro_coro::http
{

// ============================================================================
// HttpIncomingStreamBase Implementation
// ============================================================================

template <typename Derived, typename DataType>
Task<std::string_view> HttpIncomingStreamBase<Derived, DataType>::read(size_t maxSize)
{
    if (bodyBytesRead_ >= contentLength_)
        co_return std::string_view();

    size_t available = buffer_.remainSize();
    if (bodyBytesRead_ == 0 && available > 0)
    {
        size_t n = std::min(available, contentLength_);
        bodyBytesRead_ += n;
        if (bodyBytesRead_ >= contentLength_)
            complete_ = true;
        co_return buffer_.consumeView(n);
    }

    size_t toRead = std::min(maxSize, contentLength_ - bodyBytesRead_);
    char * writePtr = buffer_.prepareWrite(toRead);
    size_t n = co_await conn_->read(writePtr, toRead);
    buffer_.commitWrite(n);

    bodyBytesRead_ += n;
    if (bodyBytesRead_ >= contentLength_)
        complete_ = true;

    co_return buffer_.consumeView(n);
}

template <typename Derived, typename DataType>
Task<size_t> HttpIncomingStreamBase<Derived, DataType>::readTo(char * buf, size_t len)
{
    size_t available = buffer_.remainSize();
    if (available > 0)
    {
        size_t toRead = std::min(len, available);
        std::memcpy(buf, buffer_.view().data(), toRead);
        buffer_.consume(toRead);
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

template <typename Derived, typename DataType>
Task<std::string_view> HttpIncomingStreamBase<Derived, DataType>::readAll()
{
    if (bodyBytesRead_ >= contentLength_)
        co_return buffer_.view();

    size_t startSize = buffer_.remainSize();
    if (bodyBytesRead_ == 0 && startSize > 0)
    {
        bodyBytesRead_ += std::min(startSize, contentLength_);
    }

    while (bodyBytesRead_ < contentLength_)
    {
        constexpr size_t CHUNK_SIZE = 4096;
        size_t toRead = std::min(CHUNK_SIZE, contentLength_ - bodyBytesRead_);
        char * writePtr = buffer_.prepareWrite(toRead);
        size_t n = co_await conn_->read(writePtr, toRead);
        buffer_.commitWrite(n);
        bodyBytesRead_ += n;
    }

    if (bodyBytesRead_ >= contentLength_)
        complete_ = true;

    co_return buffer_.view();
}

// Explicit instantiations
template class HttpIncomingStreamBase<HttpIncomingStream<HttpRequest>, HttpRequest>;
template class HttpIncomingStreamBase<HttpIncomingStream<HttpResponse>, HttpResponse>;

// ============================================================================
// HttpIncomingStream<HttpRequest> Implementation
// ============================================================================

Task<> HttpIncomingStream<HttpRequest>::readAndParse()
{
    while (!headerComplete_)
    {
        size_t pos = buffer_.find("\r\n");
        if (pos == std::string::npos)
        {
            char * writePtr = buffer_.prepareWrite(4096);
            size_t n = co_await conn_->read(writePtr, 4096);
            buffer_.commitWrite(n);
            if (n == 0)
                // TODO
                co_return;
            continue;
        }

        std::string_view line = buffer_.view().substr(0, pos);
        buffer_.consume(pos + 2);

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

// ============================================================================
// HttpIncomingStream<HttpResponse> Implementation
// ============================================================================

Task<> HttpIncomingStream<HttpResponse>::readAndParse()
{
    while (!headerComplete_)
    {
        size_t pos = buffer_.find("\r\n");
        if (pos == std::string::npos)
        {
            if (buffer_.remainSize() >= MAX_HEADER_LINE_LENGTH)
            {
                // TODO: respond and shutdown
                throw std::runtime_error("Bad data, request line too long");
            }

            char * writePtr = buffer_.prepareWrite(4096);
            size_t n = co_await conn_->read(writePtr, 4096);
            buffer_.commitWrite(n);
            if (n == 0) // TODO
                co_return;
            continue;
        }

        std::string_view line = buffer_.view().substr(0, pos);
        buffer_.consume(pos + 2);

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
            auto it = data_.headers.find(std::string{ HttpHeader::Name::ContentLength_L });
            if (it != data_.headers.end())
            {
                contentLength_ = std::stoul(it->second.value());
            }
            headerComplete_ = true;
            if (contentLength_ == 0)
                complete_ = true;
        }
    }
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

Task<HttpCompleteResponse> HttpIncomingStream<HttpResponse>::toCompleteResponse()
{
    auto bodyView = co_await readAll();
    co_return HttpCompleteResponse(std::move(data_), std::string(bodyView));
}

} // namespace nitro_coro::http
