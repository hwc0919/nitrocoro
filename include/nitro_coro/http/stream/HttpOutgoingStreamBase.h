/**
 * @file HttpOutgoingStreamBase.h
 * @brief Base class for HTTP outgoing streams
 */
#pragma once
#include <nitro_coro/core/Task.h>
#include <nitro_coro/http/BodyWriter.h>
#include <nitro_coro/http/HttpParser.h>
#include <nitro_coro/net/TcpConnection.h>

#include <memory>
#include <string>
#include <string_view>

namespace nitro_coro::http
{

template <typename DataType>
class HttpOutgoingStreamBase
{
public:
    explicit HttpOutgoingStreamBase(net::TcpConnectionPtr conn)
        : conn_(std::move(conn)) {}

    void setHeader(std::string_view name, std::string value);
    void setHeader(HttpHeader::NameCode code, std::string value);
    void setHeader(HttpHeader header);
    void setCookie(const std::string & name, std::string value);
    Task<> write(const char * data, size_t len);
    Task<> write(std::string_view data);
    Task<> end();
    Task<> end(std::string_view data);

protected:
    static const char * getDefaultReason(StatusCode code);
    Task<> writeHeaders();
    void buildHeaders(std::ostringstream & oss);
    void decideTransferMode(std::optional<size_t> lengthHint = std::nullopt);

    DataType data_;
    net::TcpConnectionPtr conn_;
    bool headersSent_ = false;
    TransferMode transferMode_ = TransferMode::Chunked;
    std::unique_ptr<BodyWriter> bodyWriter_;
};

} // namespace nitro_coro::http
