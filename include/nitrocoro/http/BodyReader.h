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

    /**
     * Reads up to @p len bytes of the HTTP body into @p buf.
     *
     * @return Number of bytes read (> 0), or 0 if the body is complete.
     *         Returning 0 means the body has been fully consumed according to
     *         the transfer semantics (Content-Length satisfied, chunked
     *         terminator received, or connection closed for UntilClose mode).
     *         Note: this differs from TcpConnection::read(), where 0 means
     *         TCP EOF. Here, 0 always means "body done", regardless of the
     *         underlying transfer mode.
     * @throws std::runtime_error on I/O error or malformed HTTP body
     *         (e.g. truncated Content-Length body, invalid chunked encoding).
     */
    virtual Task<size_t> read(char * buf, size_t len) = 0;

    /** Returns true if the body has been fully consumed. */
    virtual bool isComplete() const = 0;

    /**
     * Reads the entire remaining body into @p buf.
     *
     * Calls read() repeatedly until the body is complete. read() returning 0
     * and isComplete() returning true are equivalent and always occur together.
     *
     * @return Total number of bytes appended to @p buf.
     * @throws std::runtime_error on I/O error or malformed body (propagated from read()).
     */
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
