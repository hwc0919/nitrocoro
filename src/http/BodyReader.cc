/**
 * @file BodyReaderFactory.cc
 * @brief Factory implementation for creating body readers
 */
#include "body_reader/ChunkedReader.h"
#include "body_reader/ContentLengthReader.h"
#include "body_reader/UntilCloseReader.h"
#include <nitro_coro/http/BodyReader.h>

namespace nitro_coro::http
{

std::unique_ptr<BodyReader> BodyReader::create(
    TransferMode mode,
    net::TcpConnectionPtr conn,
    utils::StringBuffer & buffer,
    size_t contentLength)
{
    switch (mode)
    {
        case TransferMode::ContentLength:
            return std::make_unique<ContentLengthReader>(std::move(conn), buffer, contentLength);
        case TransferMode::Chunked:
            return std::make_unique<ChunkedReader>(std::move(conn), buffer);
        case TransferMode::UntilClose:
            return std::make_unique<UntilCloseReader>(std::move(conn), buffer);
    }
    return std::make_unique<UntilCloseReader>(std::move(conn), buffer);
}

} // namespace nitro_coro::http
