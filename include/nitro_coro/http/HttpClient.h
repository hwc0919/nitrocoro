/**
 * @file HttpClient.h
 * @brief HTTP client for making requests
 */
#pragma once

#include <nitro_coro/core/Task.h>
#include <nitro_coro/core/Future.h>
#include <nitro_coro/http/HttpStream.h>
#include <nitro_coro/http/HttpHeader.h>
#include <nitro_coro/net/TcpConnection.h>
#include <nitro_coro/net/Url.h>

#include <map>
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
    const std::map<std::string, HttpHeader> & headers() const { return headers_; }
    std::string_view cookie(const std::string & name) const;
    const std::map<std::string, std::string> & cookies() const { return cookies_; }

private:
    friend class HttpClient;

    int statusCode_ = 0;
    std::string statusReason_;
    std::map<std::string, HttpHeader> headers_;
    std::map<std::string, std::string> cookies_;
    std::string body_;
};

struct HttpClientSession
{
    HttpOutgoingStream<HttpRequest> request;
    Future<HttpIncomingStream<HttpResponse>> response;
};

class HttpClient
{
public:
    HttpClient() = default;

    // Simple API
    Task<HttpClientResponse> get(const std::string & url);
    Task<HttpClientResponse> post(const std::string & url, const std::string & body);
    Task<HttpClientResponse> request(const std::string & method, const std::string & url, const std::string & body = "");

    // Stream API
    Task<HttpClientSession> stream(const std::string & method, const std::string & url);

private:
    Task<HttpClientResponse> sendRequest(const std::string & method, const net::Url & url, const std::string & body);
    Task<HttpClientResponse> readResponse(net::TcpConnectionPtr conn);
};

} // namespace nitro_coro::http
