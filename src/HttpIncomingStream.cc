/**
 * @file HttpIncomingStream.cc
 * @brief HTTP incoming stream implementations
 */
#include <nitro_coro/http/HttpMessage.h>
#include <nitro_coro/http/stream/HttpIncomingStream.h>

#include <cstring>

namespace nitro_coro::http
{

// ============================================================================
// HttpIncomingStreamBase Implementation
// ============================================================================

template <typename Derived, typename DataType>
Task<> HttpIncomingStreamBase<Derived, DataType>::readAndParse()
{
    while (!parser_.isHeaderComplete())
    {
        size_t pos = buffer_.find("\r\n");
        if (pos == std::string::npos)
        {
            char * writePtr = buffer_.prepareWrite(4096);
            size_t n = co_await conn_->read(writePtr, 4096);
            buffer_.commitWrite(n);
            if (n == 0)
                co_return;
            continue;
        }

        std::string_view line = buffer_.view().substr(0, pos);
        buffer_.consume(pos + 2);
        parser_.parseLine(line);
    }

    if (parser_.contentLength() == 0)
        complete_ = true;
}

template <typename Derived, typename DataType>
Task<std::string_view> HttpIncomingStreamBase<Derived, DataType>::read(size_t maxSize)
{
    size_t contentLength = parser_.contentLength();
    if (bodyBytesRead_ >= contentLength)
        co_return std::string_view();

    size_t available = buffer_.remainSize();
    if (bodyBytesRead_ == 0 && available > 0)
    {
        size_t n = std::min(available, contentLength);
        bodyBytesRead_ += n;
        if (bodyBytesRead_ >= contentLength)
            complete_ = true;
        co_return buffer_.consumeView(n);
    }

    size_t toRead = std::min(maxSize, contentLength - bodyBytesRead_);
    char * writePtr = buffer_.prepareWrite(toRead);
    size_t n = co_await conn_->read(writePtr, toRead);
    buffer_.commitWrite(n);

    bodyBytesRead_ += n;
    if (bodyBytesRead_ >= contentLength)
        complete_ = true;

    co_return buffer_.consumeView(n);
}

template <typename Derived, typename DataType>
Task<size_t> HttpIncomingStreamBase<Derived, DataType>::readTo(char * buf, size_t len)
{
    size_t contentLength = parser_.contentLength();
    size_t available = buffer_.remainSize();
    if (available > 0)
    {
        size_t toRead = std::min(len, available);
        std::memcpy(buf, buffer_.view().data(), toRead);
        buffer_.consume(toRead);
        bodyBytesRead_ += toRead;
        if (bodyBytesRead_ >= contentLength)
            complete_ = true;
        co_return toRead;
    }

    if (conn_ && bodyBytesRead_ < contentLength)
    {
        size_t remaining = contentLength - bodyBytesRead_;
        size_t toRead = std::min(len, remaining);
        size_t n = co_await conn_->read(buf, toRead);
        bodyBytesRead_ += n;
        if (bodyBytesRead_ >= contentLength)
            complete_ = true;
        co_return n;
    }

    co_return 0;
}

template <typename Derived, typename DataType>
Task<std::string_view> HttpIncomingStreamBase<Derived, DataType>::readAll()
{
    size_t contentLength = parser_.contentLength();
    if (bodyBytesRead_ >= contentLength)
        co_return buffer_.view();

    size_t startSize = buffer_.remainSize();
    if (bodyBytesRead_ == 0 && startSize > 0)
    {
        bodyBytesRead_ += std::min(startSize, contentLength);
    }

    while (bodyBytesRead_ < contentLength)
    {
        constexpr size_t CHUNK_SIZE = 4096;
        size_t toRead = std::min(CHUNK_SIZE, contentLength - bodyBytesRead_);
        char * writePtr = buffer_.prepareWrite(toRead);
        size_t n = co_await conn_->read(writePtr, toRead);
        buffer_.commitWrite(n);
        bodyBytesRead_ += n;
    }

    if (bodyBytesRead_ >= contentLength)
        complete_ = true;

    co_return buffer_.view();
}

// Explicit instantiations
template class HttpIncomingStreamBase<HttpIncomingStream<HttpRequest>, HttpRequest>;
template class HttpIncomingStreamBase<HttpIncomingStream<HttpResponse>, HttpResponse>;

// ============================================================================
// HttpIncomingStream<HttpResponse> Implementation
// ============================================================================

Task<HttpCompleteResponse> HttpIncomingStream<HttpResponse>::toCompleteResponse()
{
    auto bodyView = co_await readAll();
    co_return HttpCompleteResponse(std::move(data_), std::string(bodyView));
}

} // namespace nitro_coro::http
