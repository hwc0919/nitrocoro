/**
 * @file ContentLengthWriter.cc
 * @brief Body writer for Content-Length based transfer
 */
#include "ContentLengthWriter.h"

namespace nitrocoro::http
{

Task<> ContentLengthWriter::write(std::string_view data)
{
    co_await conn_->write(data.data(), data.size());
    bytesWritten_ += data.size();
}

Task<> ContentLengthWriter::end()
{
    co_return;
}

} // namespace nitrocoro::http
