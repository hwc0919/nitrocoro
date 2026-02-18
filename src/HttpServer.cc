/**
 * @file HttpServer.cc
 * @brief HTTP server implementation
 */
#include <nitro_coro/http/HttpServer.h>
#include <nitro_coro/utils/Debug.h>

namespace nitro_coro::http
{

HttpServer::HttpServer(uint16_t port)
    : port_(port), scheduler_(Scheduler::current())
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
    char buf[4096];

    try
    {
        HttpIncomingStream<HttpRequest> request;

        // Parse until headers are complete
        while (!request.isHeaderComplete())
        {
            size_t n = co_await conn->read(buf, sizeof(buf));
            if (n <= 0)
                co_return;

            request.parse(buf, n);
        }

        // Set connection for streaming body reads
        request.conn_ = conn;

        HttpOutgoingStream<HttpResponse> response(conn);

        auto key = std::make_pair(request.method(), request.path());
        auto it = routes_.find(key);

        if (it != routes_.end())
        {
            co_await it->second(request, response);
        }
        else
        {
            response.setStatus(404);
            co_await response.end("Not Found");
        }
    }
    catch (const std::exception & e)
    {
        NITRO_ERROR("Error handling request: %s\n", e.what());
    }
}

} // namespace nitro_coro::http
