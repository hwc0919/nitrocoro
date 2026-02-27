/**
 * @file HttpIncomingStreamBase.h
 * @brief Base class for HTTP incoming streams
 */
#pragma once
#include <nitrocoro/core/Task.h>
#include <nitrocoro/http/BodyReader.h>

#include <memory>
#include <string>

namespace nitrocoro::http
{

template <typename DataType>
class HttpIncomingStreamBase
{
public:
    HttpIncomingStreamBase(DataType message, std::shared_ptr<BodyReader> bodyReader)
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
    std::shared_ptr<BodyReader> bodyReader_;
};

} // namespace nitrocoro::http
