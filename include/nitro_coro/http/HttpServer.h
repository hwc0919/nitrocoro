/**
 * @file HttpServer.h
 * @brief HTTP server based on TcpServer
 */
#pragma once

#include <functional>
#include <map>
#include <memory>
#include <nitro_coro/core/Task.h>
#include <nitro_coro/http/HttpStream.h>
#include <nitro_coro/net/TcpServer.h>
#include <string>

namespace nitro_coro::http
{

class HttpServer
{
public:
    using Handler = std::function<Task<>(HttpIncomingStream<HttpRequest> &, HttpOutgoingStream<HttpResponse> &)>;

    explicit HttpServer(uint16_t port);

    void route(const std::string & method, const std::string & path, Handler handler);
    Task<> start();
    Task<> stop();

private:
    Task<> handleConnection(net::TcpConnectionPtr conn);

    uint16_t port_;
    Scheduler * scheduler_;
    std::map<std::pair<std::string, std::string>, Handler> routes_;
    std::unique_ptr<net::TcpServer> server_;
};

} // namespace nitro_coro::http
