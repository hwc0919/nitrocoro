/**
 * @file HttpClient.h
 * @brief HTTP client for making requests
 */
#pragma once

#include <nitrocoro/core/Future.h>
#include <nitrocoro/core/Task.h>
#include <nitrocoro/http/HttpMessage.h>
#include <nitrocoro/http/HttpStream.h>
#include <nitrocoro/net/TcpConnection.h>
#include <nitrocoro/net/Url.h>

#include <string>

namespace nitrocoro::http
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

} // namespace nitrocoro::http
