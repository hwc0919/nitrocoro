/**
 * @file HttpServer.cc
 * @brief HTTP server implementation
 */
#include <nitro_coro/http/HttpContext.h>
#include <nitro_coro/http/HttpServer.h>
#include <nitro_coro/utils/Debug.h>

namespace nitro_coro::http
{

HttpServer::HttpServer(uint16_t port, Scheduler * scheduler)
    : port_(port), scheduler_(scheduler)
{
}

void HttpServer::route(const std::string & method, const std::string & path, Handler handler)
{
    routes_[{ method, path }] = std::move(handler);
}

Task<> HttpServer::start()
{
    server_ = std::make_unique<net::TcpServer>(port_, scheduler_);
    NITRO_INFO("HTTP server listening on port %hu\n", port_);

    co_await server_->start([this](net::TcpConnectionPtr conn) -> Task<> {
        co_await handleConnection(std::move(conn));
    });
}

Task<> HttpServer::stop()
{
    if (server_)
    {
        co_await server_->stop();
    }
}

Task<> HttpServer::handleConnection(net::TcpConnectionPtr conn)
{
    try
    {
        auto buffer = std::make_shared<utils::StringBuffer>();

        while (true)
        {
            HttpContext<HttpRequest> context(conn, buffer);
            auto message = co_await context.receiveMessage();
            // bool keepAlive = message.keepAlive;
            bool keepAlive = false; // TODO: support keep-alive

            auto request = HttpIncomingStream<HttpRequest>(
                std::move(message),
                BodyReader::create(conn, buffer, message.transferMode, message.contentLength));
            HttpOutgoingStream<HttpResponse> response(conn);
            response.setCloseConnection(!keepAlive);

            auto key = std::make_pair(std::string{ request.method() }, std::string{ request.path() });
            auto it = routes_.find(key);

            if (it != routes_.end())
            {
                co_await it->second(request, response);
            }
            else
            {
                response.setStatus(StatusCode::k404NotFound);
                co_await response.end("Not Found");
            }

            if (!keepAlive)
            {
                break;
            }
            // TODO: request body must be totally consumed before processing the next request in the same connection
            co_await std::suspend_always{};
        }
    }
    catch (const std::exception & e)
    {
        NITRO_ERROR("Error handling request: %s\n", e.what());
    }
    co_await conn->close();
}

} // namespace nitro_coro::http
