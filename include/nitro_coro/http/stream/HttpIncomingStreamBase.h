/**
 * @file HttpIncomingStreamBase.h
 * @brief CRTP base class for HTTP incoming streams
 */
#pragma once
#include <nitro_coro/core/Task.h>
#include <nitro_coro/net/TcpConnection.h>
#include <nitro_coro/utils/StringBuffer.h>

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
    utils::StringBuffer buffer_;
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

    Task<std::string_view> read(size_t maxSize = 4096);
    Task<size_t> readTo(char * buf, size_t len);
    Task<std::string_view> readAll();
};

} // namespace nitro_coro::http
