/**
 * @file HttpResponse.h
 * @brief HTTP response builder
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

class HttpResponse
{
public:
    explicit HttpResponse(net::TcpConnectionPtr conn)
        : conn_(std::move(conn)) {}

    void setStatus(int code, const std::string & reason = "");
    void setHeader(const std::string & name, const std::string & value);
    void setCookie(const std::string & name, const std::string & value);

    Task<> write(const std::string & body);
    Task<> writeHeaders();

private:
    net::TcpConnectionPtr conn_;
    int statusCode_ = 200;
    std::string statusReason_;
    std::map<std::string, HttpHeader> headers_;
    std::vector<std::pair<std::string, std::string>> cookies_;
    bool headersSent_ = false;

    static const char * getDefaultReason(int code);
};

} // namespace nitro_coro::http
