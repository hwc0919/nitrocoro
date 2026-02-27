/**
 * @file ContentLengthReader.cc
 * @brief Implementation of ContentLengthReader
 */
#include "ContentLengthReader.h"
#include <cstring>

namespace nitrocoro::http
{

Task<size_t> ContentLengthReader::readImpl(char * buf, size_t len)
{
    if (bytesRead_ >= contentLength_)
        co_return 0;

    size_t available = buffer_->remainSize();
    if (available > 0)
    {
        size_t toRead = std::min({ len, available, contentLength_ - bytesRead_ });
        std::memcpy(buf, buffer_->view().data(), toRead);
        buffer_->consume(toRead);
        bytesRead_ += toRead;
        co_return toRead;
    }

    size_t remaining = contentLength_ - bytesRead_;
    size_t toRead = std::min(len, remaining);
    size_t n = co_await conn_->read(buf, toRead);
    if (n == 0)
        throw std::runtime_error("Connection closed before content-length body complete");
    bytesRead_ += n;
    co_return n;
}

} // namespace nitrocoro::http
