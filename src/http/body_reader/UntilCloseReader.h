/**
 * @file UntilCloseReader.h
 * @brief Body reader that reads until connection closes
 */
#pragma once
#include <nitro_coro/http/BodyReader.h>

namespace nitro_coro::http
{

class UntilCloseReader : public BodyReader
{
public:
    UntilCloseReader(net::TcpConnectionPtr conn, utils::StringBuffer & buffer)
        : conn_(std::move(conn)), buffer_(buffer) {}

    Task<std::string_view> read(size_t maxSize) override;
    Task<size_t> readTo(char * buf, size_t len) override;
    Task<std::string_view> readAll() override;
    bool isComplete() const override { return complete_; }

private:
    net::TcpConnectionPtr conn_;
    utils::StringBuffer & buffer_;
    bool complete_ = false;
};

} // namespace nitro_coro::http
