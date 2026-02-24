/**
 * @file UntilCloseReader.cc
 * @brief Implementation of UntilCloseReader
 */
#include "UntilCloseReader.h"
#include <cstring>

namespace nitro_coro::http
{

Task<std::string_view> UntilCloseReader::read(size_t maxSize)
{
    if (complete_)
        co_return std::string_view();

    size_t available = buffer_->remainSize();
    if (available > 0)
    {
        size_t toRead = std::min(available, maxSize);
        co_return buffer_->consumeView(toRead);
    }

    char * writePtr = buffer_->prepareWrite(maxSize);
    size_t n = co_await conn_->read(writePtr, maxSize);
    buffer_->commitWrite(n);

    if (n == 0)
        // TODO
        complete_ = true;

    co_return buffer_->consumeView(n);
}

Task<size_t> UntilCloseReader::readTo(char * buf, size_t len)
{
    if (complete_)
        co_return 0;

    size_t available = buffer_->remainSize();
    if (available > 0)
    {
        size_t toRead = std::min(len, available);
        std::memcpy(buf, buffer_->view().data(), toRead);
        buffer_->consume(toRead);
        co_return toRead;
    }

    size_t n = co_await conn_->read(buf, len);
    if (n == 0)
        complete_ = true;

    co_return n;
}

} // namespace nitro_coro::http
