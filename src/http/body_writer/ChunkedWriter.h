/**
 * @file ChunkedWriter.h
 * @brief Body writer for chunked transfer encoding
 */
#pragma once
#include <nitrocoro/http/BodyWriter.h>

namespace nitrocoro::http
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

} // namespace nitrocoro::http
