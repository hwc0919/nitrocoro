/**
 * @file HttpServer.cc
 * @brief HTTP server implementation
 */
#include <nitrocoro/http/HttpContext.h>
#include <nitrocoro/http/HttpServer.h>
#include <nitrocoro/utils/Debug.h>

namespace nitrocoro::http
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
        try
        {
            co_await handleConnection(conn);
        }
        catch (const std::exception & e)
        {
            NITRO_ERROR("Error handling connection: %s\n", e.what());
        }
        catch (...)
        {
            NITRO_ERROR("Unknown error handling connection\n");
        }
        co_await conn->close();
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
    auto buffer = std::make_shared<utils::StringBuffer>();
    HttpContext<HttpRequest> context(conn, buffer);
    while (true)
    {
        auto message = co_await context.receiveMessage();
        if (!message)
            break;
        bool keepAlive = message->keepAlive;

        auto bodyReader = BodyReader::create(conn, buffer, message->transferMode, message->contentLength);

        auto request = HttpIncomingStream<HttpRequest>(std::move(*message), bodyReader);
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
            break;

        co_await bodyReader->drain();
    }
}

} // namespace nitrocoro::http
