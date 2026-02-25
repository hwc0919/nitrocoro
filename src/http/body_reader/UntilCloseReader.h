/**
 * @file UntilCloseReader.h
 * @brief Body reader that reads until connection closes
 */
#pragma once
#include <nitrocoro/http/BodyReader.h>

namespace nitrocoro::http
{

class UntilCloseReader : public BodyReader
{
public:
    UntilCloseReader(net::TcpConnectionPtr conn, std::shared_ptr<utils::StringBuffer> buffer)
        : conn_(std::move(conn)), buffer_(std::move(buffer)) {}

    Task<size_t> readImpl(char * buf, size_t len) override;
    bool isComplete() const override { return complete_; }

private:
    net::TcpConnectionPtr conn_;
    std::shared_ptr<utils::StringBuffer> buffer_;
    bool complete_ = false;
};

} // namespace nitrocoro::http
