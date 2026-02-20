/**
 * @file HttpClient.h
 * @brief HTTP client for making requests
 */
#pragma once

#include <nitro_coro/core/Future.h>
#include <nitro_coro/core/Task.h>
#include <nitro_coro/http/HttpMessage.h>
#include <nitro_coro/http/HttpStream.h>
#include <nitro_coro/net/TcpConnection.h>
#include <nitro_coro/net/Url.h>

#include <string>

namespace nitro_coro::http
{

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
    Task<HttpCompleteResponse> get(const std::string & url);
    Task<HttpCompleteResponse> post(const std::string & url, const std::string & body);
    Task<HttpCompleteResponse> request(const std::string & method, const std::string & url, const std::string & body = "");

    // Stream API
    Task<HttpClientSession> stream(const std::string & method, const std::string & url);

private:
    Task<HttpCompleteResponse> sendRequest(const std::string & method, const net::Url & url, const std::string & body);
    Task<HttpCompleteResponse> readResponse(net::TcpConnectionPtr conn);
};

} // namespace nitro_coro::http
