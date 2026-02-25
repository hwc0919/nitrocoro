/**
 * @file ChunkedWriter.cc
 * @brief Body writer for chunked transfer encoding
 */
#include "ChunkedWriter.h"
#include <cstdio>

namespace nitrocoro::http
{

Task<> ChunkedWriter::write(std::string_view data)
{
    if (data.empty())
        co_return;

    char sizeBuf[16];
    int sizeLen = std::snprintf(sizeBuf, sizeof(sizeBuf), "%zx\r\n", data.size());

    std::string chunk;
    chunk.reserve(sizeLen + data.size() + 2);
    chunk.append(sizeBuf, sizeLen).append(data).append("\r\n", 2);
    co_await conn_->write(chunk.c_str(), chunk.size());
}

Task<> ChunkedWriter::end()
{
    co_await conn_->write("0\r\n\r\n", 5);
}

} // namespace nitrocoro::http
