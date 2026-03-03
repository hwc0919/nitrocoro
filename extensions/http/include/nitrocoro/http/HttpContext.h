/**
 * @file HttpContext.h
 * @brief HTTP context for parsing headers
 */
#pragma once
#include <nitrocoro/core/Task.h>
#include <nitrocoro/http/HttpParser.h>
#include <nitrocoro/io/AnyStream.h>
#include <nitrocoro/utils/StringBuffer.h>

#include <memory>
#include <optional>

namespace nitrocoro::http
{

template <typename MessageType>
class HttpContext
{
public:
    HttpContext(io::AnyStreamPtr stream, std::shared_ptr<utils::StringBuffer> buffer)
        : stream_(std::move(stream)), buffer_(std::move(buffer))
    {
    }

    Task<std::optional<MessageType>> receiveMessage();

    io::AnyStreamPtr stream() const { return stream_; }
    std::shared_ptr<utils::StringBuffer> buffer() const { return buffer_; }

private:
    io::AnyStreamPtr stream_;
    std::shared_ptr<utils::StringBuffer> buffer_;
};

} // namespace nitrocoro::http
