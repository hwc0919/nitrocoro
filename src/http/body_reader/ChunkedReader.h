/**
 * @file ChunkedReader.h
 * @brief Body reader for chunked transfer encoding
 */
#pragma once
#include <nitro_coro/http/BodyReader.h>

namespace nitro_coro::http
{

class ChunkedReader : public BodyReader
{
public:
    ChunkedReader(net::TcpConnectionPtr conn, std::shared_ptr<utils::StringBuffer> buffer)
        : conn_(std::move(conn)), buffer_(std::move(buffer)) {}

    Task<std::string_view> read(size_t maxSize) override;
    Task<size_t> readTo(char * buf, size_t len) override;
    Task<std::string_view> readAll() override;
    bool isComplete() const override { return complete_; }

private:
    enum class State { ReadSize, ReadData, Complete };
    
    net::TcpConnectionPtr conn_;
    std::shared_ptr<utils::StringBuffer> buffer_;
    State state_ = State::ReadSize;
    size_t currentChunkSize_ = 0;
    size_t currentChunkRead_ = 0;
    bool complete_ = false;

    Task<bool> parseChunkSize();
    Task<> skipCRLF();
};

} // namespace nitro_coro::http
