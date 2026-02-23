/**
 * @file ContentLengthReader.h
 * @brief Body reader for Content-Length based transfer
 */
#pragma once
#include <nitro_coro/http/BodyReader.h>

namespace nitro_coro::http
{

class ContentLengthReader : public BodyReader
{
public:
    ContentLengthReader(net::TcpConnectionPtr conn, std::shared_ptr<utils::StringBuffer> buffer, size_t contentLength)
        : conn_(std::move(conn)), buffer_(std::move(buffer)), contentLength_(contentLength) {}

    Task<std::string_view> read(size_t maxSize) override;
    Task<size_t> readTo(char * buf, size_t len) override;
    Task<std::string_view> readAll() override;
    bool isComplete() const override { return bytesRead_ >= contentLength_; }

private:
    net::TcpConnectionPtr conn_;
    std::shared_ptr<utils::StringBuffer> buffer_;
    const size_t contentLength_;
    size_t bytesRead_ = 0;
};

} // namespace nitro_coro::http
