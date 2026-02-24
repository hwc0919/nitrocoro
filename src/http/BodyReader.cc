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
    net::TcpConnectionPtr conn,
    std::shared_ptr<utils::StringBuffer> buffer,
    TransferMode mode,
    size_t contentLength)
{
    switch (mode)
    {
        case TransferMode::ContentLength:
            return std::make_unique<ContentLengthReader>(std::move(conn), std::move(buffer), contentLength);
        case TransferMode::Chunked:
            return std::make_unique<ChunkedReader>(std::move(conn), std::move(buffer));
        case TransferMode::UntilClose:
            return std::make_unique<UntilCloseReader>(std::move(conn), std::move(buffer));
    }
    return std::make_unique<UntilCloseReader>(std::move(conn), std::move(buffer));
}

} // namespace nitro_coro::http
