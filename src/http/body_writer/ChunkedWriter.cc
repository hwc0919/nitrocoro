/**
 * @file ChunkedWriter.cc
 * @brief Body writer for chunked transfer encoding
 */
#include "ChunkedWriter.h"
#include <sstream>

namespace nitro_coro::http
{

Task<> ChunkedWriter::write(std::string_view data)
{
    if (data.empty())
        co_return;

    std::ostringstream oss;
    oss << std::hex << data.size() << "\r\n";
    std::string header = oss.str();

    co_await conn_->write(header.c_str(), header.size());
    co_await conn_->write(data.data(), data.size());
    co_await conn_->write("\r\n", 2);
}

Task<> ChunkedWriter::end()
{
    co_await conn_->write("0\r\n\r\n", 5);
}

} // namespace nitro_coro::http
