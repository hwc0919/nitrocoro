/**
 * @file HttpClient.h
 * @brief HTTP client for making requests
 */
#pragma once

#include <nitro_coro/core/Task.h>
#include <nitro_coro/http/HttpRequest.h>
#include <nitro_coro/net/TcpConnection.h>
#include <string>

namespace nitro_coro::http
{

class HttpClientResponse
{
public:
    int statusCode() const { return statusCode_; }
    const std::string & statusReason() const { return statusReason_; }
    const std::string & body() const { return body_; }
    std::string_view header(const std::string & name) const;

private:
    friend class HttpClient;

    int statusCode_ = 0;
    std::string statusReason_;
    std::map<std::string, std::string> headers_;
    std::string body_;
};

class HttpClient
{
public:
    HttpClient() = default;

    Task<HttpClientResponse> get(const std::string & url);
    Task<HttpClientResponse> post(const std::string & url, const std::string & body);
    Task<HttpClientResponse> request(const std::string & method, const std::string & url, const std::string & body = "");

private:
    struct UrlParts
    {
        std::string host;
        uint16_t port;
        std::string path;
    };

    UrlParts parseUrl(const std::string & url);
    Task<HttpClientResponse> sendRequest(const std::string & method, const UrlParts & url, const std::string & body);
    Task<HttpClientResponse> readResponse(net::TcpConnectionPtr conn);
};

} // namespace nitro_coro::http
