/**
 * @file UntilCloseWriter.cc
 * @brief Body writer for close-delimited transfer (HTTP/1.0 fallback)
 */
#include "UntilCloseWriter.h"

namespace nitrocoro::http
{

Task<> UntilCloseWriter::write(std::string_view data)
{
    co_await conn_->write(data.data(), data.size());
}

Task<> UntilCloseWriter::end()
{
    co_return;
}

} // namespace nitrocoro::http
