/**
 * @file HttpIncomingStream.cc
 * @brief HTTP incoming stream implementation
 */
#include <algorithm>
#include <cstring>
#include <nitro_coro/http/HttpIncomingStream.h>
#include <sstream>

namespace nitro_coro::http
{

int HttpIncomingStream::parse(const char * data, size_t len)
{
    buffer_.append(data, len);

    while (!complete_)
    {
        if (state_ == State::RequestLine || state_ == State::Headers)
        {
            size_t pos = buffer_.find("\r\n");
            if (pos == std::string::npos)
                return len;

            std::string line = buffer_.substr(0, pos);
            buffer_.erase(0, pos + 2);

            if (state_ == State::RequestLine)
            {
                parseRequestLine(line);
                state_ = State::Headers;
            }
            else if (!line.empty())
            {
                parseHeader(line);
            }
            else
            {
                static const std::string contentLengthKey{ HttpHeader::codeToName(HttpHeader::NameCode::ContentLength) };
                auto it = headers_.find(contentLengthKey);
                if (it != headers_.end())
                {
                    contentLength_ = std::stoul(it->second.value());
                    state_ = State::Body;
                }
                else
                {
                    state_ = State::Complete;
                    complete_ = true;
                }
            }
        }
        else if (state_ == State::Body)
        {
            // Body data in buffer, keep it for streaming reads
            return len;
        }
    }

    return len;
}

void HttpIncomingStream::parseRequestLine(std::string_view line)
{
    size_t pos1 = line.find(' ');
    size_t pos2 = line.find(' ', pos1 + 1);

    method_ = line.substr(0, pos1);
    std::string fullPath(line.substr(pos1 + 1, pos2 - pos1 - 1));
    version_ = line.substr(pos2 + 1);

    size_t qpos = fullPath.find('?');
    if (qpos != std::string::npos)
    {
        path_ = fullPath.substr(0, qpos);
        parseQueryString();
    }
    else
    {
        path_ = fullPath;
    }
}

void HttpIncomingStream::parseHeader(std::string_view line)
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
        headers_.emplace(header.name(), std::move(header));
    }
}

void HttpIncomingStream::parseQueryString()
{
    size_t qpos = path_.find('?');
    if (qpos == std::string::npos)
        return;

    std::string queryStr = path_.substr(qpos + 1);
    path_ = path_.substr(0, qpos);

    std::istringstream iss(queryStr);
    std::string pair;
    while (std::getline(iss, pair, '&'))
    {
        size_t eqPos = pair.find('=');
        if (eqPos != std::string::npos)
        {
            queries_[pair.substr(0, eqPos)] = pair.substr(eqPos + 1);
        }
    }
}

std::string_view HttpIncomingStream::getHeader(const std::string & name) const
{
    std::string lowerName = name;
    std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(),
                   [](unsigned char c) { return std::tolower(c); });

    auto it = headers_.find(lowerName);
    return it != headers_.end() ? std::string_view(it->second.value()) : std::string_view();
}

std::string_view HttpIncomingStream::getQuery(const std::string & name) const
{
    auto it = queries_.find(name);
    return it != queries_.end() ? std::string_view(it->second) : std::string_view();
}

std::string_view HttpIncomingStream::getCookie(const std::string & name) const
{
    auto it = cookies_.find(name);
    return it != cookies_.end() ? std::string_view(it->second) : std::string_view();
}

void HttpIncomingStream::parseCookies(const std::string & cookieHeader)
{
    // TODO: Implement cookie parsing
}

Task<std::string_view> HttpIncomingStream::read(size_t maxSize)
{
    if (bodyBytesRead_ >= contentLength_)
    {
        co_return std::string_view();
    }

    size_t oldSize = buffer_.size();
    size_t toRead = std::min(maxSize, contentLength_ - bodyBytesRead_);

    buffer_.reserve(oldSize + toRead);
    size_t n = co_await conn_->read(buffer_.data() + oldSize, toRead);
    buffer_.resize(oldSize + n);

    bodyBytesRead_ += n;
    if (bodyBytesRead_ >= contentLength_)
    {
        complete_ = true;
    }

    co_return std::string_view(buffer_.data() + oldSize, n);
}

Task<size_t> HttpIncomingStream::readTo(char * buf, size_t len)
{
    // First, consume from buffer
    if (!buffer_.empty())
    {
        size_t toRead = std::min(len, buffer_.size());
        std::memcpy(buf, buffer_.data(), toRead);
        buffer_.erase(0, toRead);
        bodyBytesRead_ += toRead;

        if (bodyBytesRead_ >= contentLength_)
        {
            complete_ = true;
        }

        co_return toRead;
    }

    // Then read from connection
    if (conn_ && bodyBytesRead_ < contentLength_)
    {
        size_t remaining = contentLength_ - bodyBytesRead_;
        size_t toRead = std::min(len, remaining);
        size_t n = co_await conn_->read(buf, toRead);

        bodyBytesRead_ += n;
        if (bodyBytesRead_ >= contentLength_)
        {
            complete_ = true;
        }

        co_return n;
    }

    co_return 0;
}

Task<std::string_view> HttpIncomingStream::readAll()
{
    if (bodyBytesRead_ >= contentLength_)
    {
        co_return std::string_view(buffer_);
    }

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
    {
        complete_ = true;
    }

    co_return std::string_view(buffer_.data() + oldSize, buffer_.size() - oldSize);
}

} // namespace nitro_coro::http
