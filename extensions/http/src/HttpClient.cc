/**
 * @file HttpClient.cc
 * @brief HTTP client implementation
 */
#include <nitrocoro/core/Future.h>
#include <nitrocoro/core/Scheduler.h>
#include <nitrocoro/http/HttpClient.h>
#include <nitrocoro/http/HttpContext.h>
#include <nitrocoro/http/HttpMessage.h>
#include <nitrocoro/net/Dns.h>
#include <nitrocoro/net/Url.h>
#include <stdexcept>

namespace nitrocoro::http
{

Task<HttpCompleteResponse> HttpClient::get(const std::string & url)
{
    co_return co_await request("GET", url);
}

Task<HttpCompleteResponse> HttpClient::post(const std::string & url, const std::string & body)
{
    co_return co_await request("POST", url, body);
}

Task<HttpCompleteResponse> HttpClient::request(const std::string & method, const std::string & url, const std::string & body)
{
    net::Url parsedUrl(url);
    if (!parsedUrl.isValid())
        throw std::invalid_argument("Invalid URL");
    co_return co_await sendRequest(method, parsedUrl, body);
}

Task<HttpCompleteResponse> HttpClient::sendRequest(const std::string & method, const net::Url & url, const std::string & body)
{
    // Resolve hostname
    auto addresses = co_await net::resolve(url.host());
    if (addresses.empty())
        throw std::runtime_error("DNS resolution returned no addresses");

    // Try to connect to first address
    auto addr = addresses[0];
    auto conn = co_await net::TcpConnection::connect(net::InetAddress(addr.toIp(), url.port(), addr.isIpV6()));

    // Build request
    std::string request;
    request.reserve(method.size() + url.path().size() + url.host().size() + body.size() + 64);
    request.append(method).append(" ").append(url.path()).append(" HTTP/1.1\r\n");
    request.append("Host: ").append(url.host()).append("\r\n");
    request.append("Connection: close\r\n");

    if (!body.empty())
    {
        request.append("Content-Length: ").append(std::to_string(body.size())).append("\r\n");
    }

    request.append("\r\n");

    if (!body.empty())
    {
        request.append(body);
    }
    co_await conn->write(request.c_str(), request.size());

    co_return co_await readResponse(conn);
}

Task<HttpCompleteResponse> HttpClient::readResponse(net::TcpConnectionPtr conn)
{
    auto buffer = std::make_shared<utils::StringBuffer>();
    HttpContext<HttpResponse> context(std::move(conn), buffer);
    auto message = co_await context.receiveMessage();
    if (!message)
        throw std::runtime_error("Connection closed before response complete");

    auto stream = HttpIncomingStream<HttpResponse>(
        std::move(*message),
        BodyReader::create(context.connection(), buffer, message->transferMode, message->contentLength));
    co_return co_await stream.toCompleteResponse();
}

Task<HttpClientSession> HttpClient::stream(const std::string & method, const std::string & url)
{
    net::Url parsedUrl(url);
    if (!parsedUrl.isValid())
        throw std::invalid_argument("Invalid URL");

    // Resolve and connect
    auto addresses = co_await net::resolve(parsedUrl.host());
    if (addresses.empty())
        throw std::runtime_error("DNS resolution failed");

    auto conn = co_await net::TcpConnection::connect(net::InetAddress(addresses[0].toIp(), parsedUrl.port(), addresses[0].isIpV6()));

    // Create outgoing stream for request body
    HttpOutgoingStream<HttpRequest> requestStream(conn);
    requestStream.setMethod(method);
    requestStream.setPath(parsedUrl.path());
    requestStream.setHeader(HttpHeader::NameCode::Host, parsedUrl.host());

    // Create promise/future for response
    Promise<HttpIncomingStream<HttpResponse>> promise(Scheduler::current());
    auto responseFuture = promise.get_future();

    // Spawn background task to receive response
    Scheduler::current()->spawn([conn, promise = std::move(promise)]() mutable -> Task<> {
        try
        {
            auto buffer = std::make_shared<utils::StringBuffer>();
            HttpContext<HttpResponse> context(conn, buffer);
            auto message = co_await context.receiveMessage();
            if (!message)
            {
                promise.set_exception(std::make_exception_ptr(std::runtime_error("Connection closed before response complete")));
                co_return;
            }

            auto response = HttpIncomingStream<HttpResponse>(
                std::move(*message),
                BodyReader::create(context.connection(), buffer, message->transferMode, message->contentLength));
            promise.set_value(std::move(response));
        }
        catch (...)
        {
            promise.set_exception(std::current_exception());
        }
    });

    co_return HttpClientSession{ std::move(requestStream), std::move(responseFuture) };
}

} // namespace nitrocoro::http
