/**
 * @file ChunkedWriter.h
 * @brief Body writer for chunked transfer encoding
 */
#pragma once
#include <nitro_coro/http/BodyWriter.h>

namespace nitro_coro::http
{

class ChunkedWriter : public BodyWriter
{
public:
    explicit ChunkedWriter(net::TcpConnectionPtr conn)
        : conn_(std::move(conn)) {}

    Task<> write(std::string_view data) override;
    Task<> end() override;

private:
    net::TcpConnectionPtr conn_;
};

} // namespace nitro_coro::http
