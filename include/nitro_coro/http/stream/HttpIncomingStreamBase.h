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
public:
    HttpIncomingStreamBase(DataType message, std::unique_ptr<BodyReader> bodyReader)
        : data_(std::move(message)), bodyReader_(std::move(bodyReader)) {}

    const DataType & getData() const { return data_; }

    Task<size_t> read(char * buf, size_t maxLen);
    Task<std::string> read(size_t maxLen);

    template <utils::ExtendableBuffer T>
    Task<size_t> readToEnd(T & buf)
    {
        co_return co_await bodyReader_->readToEnd(buf);
    }

protected:
    DataType data_;
    std::unique_ptr<BodyReader> bodyReader_;
};

} // namespace nitro_coro::http
