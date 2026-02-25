/**
 * @file UntilCloseReader.cc
 * @brief Implementation of UntilCloseReader
 */
#include "UntilCloseReader.h"
#include <cstring>

namespace nitrocoro::http
{

Task<size_t> UntilCloseReader::readImpl(char * buf, size_t len)
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

} // namespace nitrocoro::http
