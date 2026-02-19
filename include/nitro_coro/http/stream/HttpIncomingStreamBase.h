/**
 * @file HttpIncomingStreamBase.h
 * @brief CRTP base class for HTTP incoming streams
 */
#pragma once

#include <algorithm>
#include <cstring>
#include <nitro_coro/core/Task.h>
#include <nitro_coro/http/HttpMessage.h>
#include <nitro_coro/net/TcpConnection.h>
#include <string>
#include <string_view>

namespace nitro_coro::http
{

template <typename Derived, typename DataType>
class HttpIncomingStreamBase
{
protected:
    DataType data_;
    net::TcpConnectionPtr conn_;
    std::string buffer_;
    bool headerComplete_ = false;
    bool complete_ = false;
    size_t contentLength_ = 0;
    size_t bodyBytesRead_ = 0;

    explicit HttpIncomingStreamBase(net::TcpConnectionPtr conn)
        : conn_(std::move(conn)) {}

public:
    bool isHeaderComplete() const { return headerComplete_; }
    bool isComplete() const { return complete_; }
    bool hasBody() const { return contentLength_ > 0; }
    size_t contentLength() const { return contentLength_; }
    const std::map<std::string, HttpHeader> & headers() const { return data_.headers; }
    const std::map<std::string, std::string> & cookies() const { return data_.cookies; }

    std::string_view getHeader(const std::string & name) const
    {
        std::string lowerName = name;
        std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(),
                       [](unsigned char c) { return std::tolower(c); });
        auto it = data_.headers.find(lowerName);
        return it != data_.headers.end() ? std::string_view(it->second.value()) : std::string_view();
    }

    std::string_view getHeader(HttpHeader::NameCode code) const
    {
        auto name = HttpHeader::codeToName(code);
        auto it = data_.headers.find(std::string(name));
        return it != data_.headers.end() ? std::string_view(it->second.value()) : std::string_view();
    }

    std::string_view getCookie(const std::string & name) const
    {
        auto it = data_.cookies.find(name);
        return it != data_.cookies.end() ? std::string_view(it->second) : std::string_view();
    }

    Task<std::string_view> read(size_t maxSize = 4096)
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

    Task<size_t> readTo(char * buf, size_t len)
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

    Task<std::string_view> readAll()
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
};

} // namespace nitro_coro::http
