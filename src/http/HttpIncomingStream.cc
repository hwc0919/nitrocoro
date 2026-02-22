/**
 * @file HttpIncomingStream.cc
 * @brief HTTP incoming stream implementations
 */
#include <nitro_coro/http/BodyReader.h>
#include <nitro_coro/http/HttpMessage.h>
#include <nitro_coro/http/stream/HttpIncomingStream.h>

namespace nitro_coro::http
{

// ============================================================================
// HttpIncomingStreamBase Implementation
// ============================================================================

template <typename DataType>
Task<> HttpIncomingStreamBase<DataType>::readAndParse()
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

    bodyReader_ = BodyReader::create(
        parser_.transferMode(),
        conn_,
        buffer_,
        parser_.contentLength());
}

template <typename DataType>
Task<std::string_view> HttpIncomingStreamBase<DataType>::read(size_t maxSize)
{
    co_return co_await bodyReader_->read(maxSize);
}

template <typename DataType>
Task<size_t> HttpIncomingStreamBase<DataType>::readTo(char * buf, size_t len)
{
    co_return co_await bodyReader_->readTo(buf, len);
}

template <typename DataType>
Task<std::string_view> HttpIncomingStreamBase<DataType>::readAll()
{
    co_return co_await bodyReader_->readAll();
}

// Explicit instantiations
template class HttpIncomingStreamBase<HttpRequest>;
template class HttpIncomingStreamBase<HttpResponse>;

// ============================================================================
// HttpIncomingStream<HttpResponse> Implementation
// ============================================================================

Task<HttpCompleteResponse> HttpIncomingStream<HttpResponse>::toCompleteResponse()
{
    auto bodyView = co_await readAll();
    co_return HttpCompleteResponse(std::move(data_), std::string(bodyView));
}

} // namespace nitro_coro::http
