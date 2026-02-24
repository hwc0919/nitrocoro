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
Task<size_t> HttpIncomingStreamBase<DataType>::read(char * buf, size_t len)
{
    co_return co_await bodyReader_->read(buf, len);
}

template <typename DataType>
Task<std::string> HttpIncomingStreamBase<DataType>::read(size_t maxLen)
{
    std::string result(maxLen, '\0');
    size_t n = co_await read(result.data(), maxLen);
    result.resize(n);
    co_return result;
}

// Explicit instantiations
template class HttpIncomingStreamBase<HttpRequest>;
template class HttpIncomingStreamBase<HttpResponse>;

// ============================================================================
// HttpIncomingStream<HttpResponse> Implementation
// ============================================================================

Task<HttpCompleteResponse> HttpIncomingStream<HttpResponse>::toCompleteResponse()
{
    utils::StringBuffer bodyBuf;
    co_await readToEnd(bodyBuf);
    co_return HttpCompleteResponse(std::move(data_), bodyBuf.extract());
}

} // namespace nitro_coro::http
