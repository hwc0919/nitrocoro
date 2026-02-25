/**
 * @file BodyReader.h
 * @brief Body reader interface and factory
 */
#pragma once
#include <nitrocoro/core/Task.h>
#include <nitrocoro/http/HttpParser.h>
#include <nitrocoro/net/TcpConnection.h>
#include <nitrocoro/utils/ExtendableBuffer.h>
#include <nitrocoro/utils/StringBuffer.h>

#include <memory>
#include <string_view>

namespace nitrocoro::http
{

class BodyReader
{
public:
    static std::unique_ptr<BodyReader> create(
        net::TcpConnectionPtr conn,
        std::shared_ptr<utils::StringBuffer> buffer,
        TransferMode mode,
        size_t contentLength);

    virtual ~BodyReader() = default;

    virtual Task<size_t> read(char * buf, size_t len) = 0;
    virtual bool isComplete() const = 0;

    // New interface: read to end with extendable buffer
    template <utils::ExtendableBuffer T>
    Task<size_t> readToEnd(T & buf);
};

// Template method implementation
template <utils::ExtendableBuffer T>
Task<size_t> BodyReader::readToEnd(T & buf)
{
    size_t total = 0;
    while (!isComplete())
    {
        size_t available = buf.writableSize();
        char * ptr;

        if (available < 4096)
        {
            ptr = buf.prepareWrite(4096);
            available = 4096;
        }
        else
        {
            ptr = buf.beginWrite();
        }

        size_t n = co_await read(ptr, available);
        if (n == 0)
            break;

        buf.commitWrite(n);
        total += n;
    }
    co_return total;
}

} // namespace nitrocoro::http
