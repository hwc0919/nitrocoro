/**
 * @file BodyWriter.cc
 * @brief Factory implementation for creating body writers
 */
#include "body_writer/ChunkedWriter.h"
#include "body_writer/ContentLengthWriter.h"
#include <nitro_coro/http/BodyWriter.h>

namespace nitro_coro::http
{

std::unique_ptr<BodyWriter> BodyWriter::create(
    TransferMode mode,
    net::TcpConnectionPtr conn,
    size_t contentLength)
{
    if (mode == TransferMode::ContentLength)
        return std::make_unique<ContentLengthWriter>(std::move(conn), contentLength);
    return std::make_unique<ChunkedWriter>(std::move(conn));
}

} // namespace nitro_coro::http
