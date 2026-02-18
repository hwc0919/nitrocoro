/**
 * @file HttpOutgoingStream.h
 * @brief HTTP outgoing message stream (for writing requests/responses)
 */
#pragma once

#include <map>
#include <memory>
#include <nitro_coro/core/Task.h>
#include <nitro_coro/http/HttpHeader.h>
#include <nitro_coro/net/TcpConnection.h>
#include <string>

namespace nitro_coro::http
{

class HttpOutgoingStream
{
public:
    explicit HttpOutgoingStream(net::TcpConnectionPtr conn)
        : conn_(std::move(conn)) {}

    // Set metadata (must be called before write)
    void setStatus(int code, const std::string & reason = "");
    void setHeader(const std::string & name, const std::string & value);
    void setCookie(const std::string & name, const std::string & value);

    // Stream writing
    Task<> write(const char * data, size_t len);
    Task<> write(std::string_view data);

    // End response
    Task<> end();
    Task<> end(std::string_view data);

private:
    Task<> writeHeaders();

    net::TcpConnectionPtr conn_;
    int statusCode_ = 200;
    std::string statusReason_;
    std::map<std::string, HttpHeader> headers_;
    std::vector<std::pair<std::string, std::string>> cookies_;
    bool headersSent_ = false;

    static const char * getDefaultReason(int code);
};

} // namespace nitro_coro::http
