/**
 * @file ContentLengthWriter.h
 * @brief Body writer for Content-Length based transfer
 */
#pragma once
#include <nitro_coro/http/BodyWriter.h>

namespace nitro_coro::http
{

class ContentLengthWriter : public BodyWriter
{
public:
    ContentLengthWriter(net::TcpConnectionPtr conn, size_t contentLength)
        : conn_(std::move(conn)), contentLength_(contentLength) {}

    Task<> write(std::string_view data) override;
    Task<> end() override;

private:
    net::TcpConnectionPtr conn_;
    const size_t contentLength_;
    size_t bytesWritten_ = 0;
};

} // namespace nitro_coro::http
