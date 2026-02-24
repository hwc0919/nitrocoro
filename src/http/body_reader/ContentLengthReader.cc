/**
 * @file ContentLengthReader.cc
 * @brief Implementation of ContentLengthReader
 */
#include "ContentLengthReader.h"
#include <cstring>

namespace nitro_coro::http
{

Task<std::string_view> ContentLengthReader::read(size_t maxSize)
{
    if (bytesRead_ >= contentLength_)
        co_return std::string_view();

    size_t available = buffer_->remainSize();
    if (bytesRead_ == 0 && available > 0)
    {
        size_t n = std::min(available, contentLength_);
        bytesRead_ += n;
        co_return buffer_->consumeView(n);
    }

    size_t toRead = std::min(maxSize, contentLength_ - bytesRead_);
    char * writePtr = buffer_->prepareWrite(toRead);
    size_t n = co_await conn_->read(writePtr, toRead);
    buffer_->commitWrite(n);
    bytesRead_ += n;

    co_return buffer_->consumeView(n);
}

Task<size_t> ContentLengthReader::readTo(char * buf, size_t len)
{
    size_t available = buffer_->remainSize();
    if (available > 0)
    {
        size_t toRead = std::min(len, available);
        std::memcpy(buf, buffer_->view().data(), toRead);
        buffer_->consume(toRead);
        bytesRead_ += toRead;
        co_return toRead;
    }

    if (conn_ && bytesRead_ < contentLength_)
    {
        size_t remaining = contentLength_ - bytesRead_;
        size_t toRead = std::min(len, remaining);
        size_t n = co_await conn_->read(buf, toRead);
        bytesRead_ += n;
        co_return n;
    }

    co_return 0;
}

Task<size_t> ContentLengthReader::read1(char* buf, size_t len)
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
    bytesRead_ += n;
    co_return n;
}

Task<std::string_view> ContentLengthReader::readAll()
{
    if (bytesRead_ >= contentLength_)
        co_return buffer_->view();

    size_t startSize = buffer_->remainSize();
    if (bytesRead_ == 0 && startSize > 0)
        bytesRead_ += std::min(startSize, contentLength_);

    while (bytesRead_ < contentLength_)
    {
        constexpr size_t MAX_BATCH_SIZE = 4096;
        size_t toRead = std::min(MAX_BATCH_SIZE, contentLength_ - bytesRead_);
        char * writePtr = buffer_->prepareWrite(toRead);
        size_t n = co_await conn_->read(writePtr, toRead);
        buffer_->commitWrite(n);
        bytesRead_ += n;
    }

    co_return buffer_->view();
}

} // namespace nitro_coro::http
