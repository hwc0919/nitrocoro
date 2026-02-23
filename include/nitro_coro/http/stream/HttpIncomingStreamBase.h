/**
 * @file HttpIncomingStreamBase.h
 * @brief Base class for HTTP incoming streams
 */
#pragma once
#include <nitro_coro/core/Task.h>
#include <nitro_coro/http/BodyReader.h>
#include <nitro_coro/http/HttpParser.h>
#include <nitro_coro/net/TcpConnection.h>
#include <nitro_coro/utils/StringBuffer.h>

#include <memory>
#include <string_view>

namespace nitro_coro::http
{

template <typename DataType>
class HttpIncomingStreamBase
{
    template <typename, typename>
    friend class HttpDataAccessor;

public:
    Task<> readAndParse();
    Task<std::string_view> read(size_t maxSize = 4096);
    Task<size_t> readTo(char * buf, size_t len);
    Task<std::string_view> readAll();

protected:
    explicit HttpIncomingStreamBase(net::TcpConnectionPtr conn)
        : parser_(data_), conn_(std::move(conn)), buffer_(std::make_shared<utils::StringBuffer>()) {}

    const DataType & getData() const { return data_; }

    DataType data_;
    HttpParser<DataType> parser_;
    net::TcpConnectionPtr conn_;
    std::shared_ptr<utils::StringBuffer> buffer_;
    std::unique_ptr<BodyReader> bodyReader_;
};

} // namespace nitro_coro::http
