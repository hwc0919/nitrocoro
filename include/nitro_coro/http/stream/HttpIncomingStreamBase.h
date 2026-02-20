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
    template <typename, typename>
    friend class HttpDataAccessor;

protected:
    static constexpr size_t MAX_HEADER_LINE_LENGTH = 1024 * 1024;
    static constexpr size_t MAX_REQUEST_LINE_LENGTH = 1024 * 1024;

    DataType data_;
    net::TcpConnectionPtr conn_;
    std::string buffer_;
    bool headerComplete_ = false;
    bool complete_ = false;
    size_t contentLength_ = 0;
    size_t bodyBytesRead_ = 0;

    explicit HttpIncomingStreamBase(net::TcpConnectionPtr conn)
        : conn_(std::move(conn)) {}

    const DataType & getData() const { return data_; }

public:
    bool isHeaderComplete() const { return headerComplete_; }
    bool isComplete() const { return complete_; }
    bool hasBody() const { return contentLength_ > 0; }
    size_t contentLength() const { return contentLength_; }

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
        if (bodyBytesRead_ == 0 && oldSize > 0)
        {
            bodyBytesRead_ += std::min(oldSize, contentLength_);
            oldSize = 0;
        }

        while (bodyBytesRead_ < contentLength_)
        {
            constexpr size_t CHUNK_SIZE = 4096;
            size_t currentSize = buffer_.size();
            size_t toRead = std::min(CHUNK_SIZE, contentLength_ - bodyBytesRead_);
            buffer_.resize(currentSize + toRead);
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
