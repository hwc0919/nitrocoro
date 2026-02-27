/**
 * @file UntilCloseWriter.h
 * @brief Body writer for close-delimited transfer (HTTP/1.0 fallback)
 */
#pragma once
#include <nitrocoro/http/BodyWriter.h>

namespace nitrocoro::http
{

class UntilCloseWriter : public BodyWriter
{
public:
    explicit UntilCloseWriter(net::TcpConnectionPtr conn)
        : conn_(std::move(conn)) {}

    Task<> write(std::string_view data) override;
    Task<> end() override;

private:
    net::TcpConnectionPtr conn_;
};

} // namespace nitrocoro::http
