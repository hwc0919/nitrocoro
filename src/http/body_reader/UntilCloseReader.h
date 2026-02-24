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
    UntilCloseReader(net::TcpConnectionPtr conn, std::shared_ptr<utils::StringBuffer> buffer)
        : conn_(std::move(conn)), buffer_(std::move(buffer)) {}

    Task<size_t> read(char * buf, size_t len) override;
    bool isComplete() const override { return complete_; }

private:
    net::TcpConnectionPtr conn_;
    std::shared_ptr<utils::StringBuffer> buffer_;
    bool complete_ = false;
};

} // namespace nitro_coro::http
