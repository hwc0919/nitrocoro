/**
 * @file BodyReader.h
 * @brief Body reader interface and factory
 */
#pragma once
#include <nitro_coro/core/Task.h>
#include <nitro_coro/http/HttpParser.h>
#include <nitro_coro/net/TcpConnection.h>
#include <nitro_coro/utils/ExtendableBuffer.h>
#include <nitro_coro/utils/StringBuffer.h>

#include <memory>
#include <string_view>

namespace nitro_coro::http
{

class BodyReader
{
public:
    static std::unique_ptr<BodyReader> create(
        TransferMode mode,
        net::TcpConnectionPtr conn,
        std::shared_ptr<utils::StringBuffer> buffer,
        size_t contentLength);

    virtual ~BodyReader() = default;

    virtual Task<std::string_view> read(size_t maxSize) = 0;
    virtual Task<size_t> readTo(char * buf, size_t len) = 0;
    virtual Task<std::string_view> readAll() = 0;
    virtual bool isComplete() const = 0;

    // New interface: read to fixed buffer
    virtual Task<size_t> read1(char * buf, size_t len) = 0;

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

        size_t n = co_await read1(ptr, available);
        if (n == 0)
            break;

        buf.commitWrite(n);
        total += n;
    }
    co_return total;
}

} // namespace nitro_coro::http
