/**
 * @file HttpClient.cc
 * @brief HTTP client implementation
 */
#include <nitro_coro/core/Future.h>
#include <nitro_coro/core/Scheduler.h>
#include <nitro_coro/http/HttpClient.h>
#include <nitro_coro/http/HttpCompleteMessage.h>
#include <nitro_coro/net/Dns.h>
#include <nitro_coro/net/Url.h>
#include <sstream>
#include <stdexcept>

namespace nitro_coro::http
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
    auto conn = co_await net::TcpConnection::connect(addr.toIp().c_str(), url.port());

    // Build request
    std::ostringstream oss;
    oss << method << " " << url.path() << " HTTP/1.1\r\n";
    oss << "Host: " << url.host() << "\r\n";
    oss << "Connection: close\r\n";

    if (!body.empty())
    {
        oss << "Content-Length: " << body.size() << "\r\n";
    }

    oss << "\r\n";

    if (!body.empty())
    {
        oss << body;
    }

    std::string request = oss.str();
    co_await conn->write(request.c_str(), request.size());

    co_return co_await readResponse(conn);
}

Task<HttpCompleteResponse> HttpClient::readResponse(net::TcpConnectionPtr conn)
{
    HttpIncomingStream<HttpResponse> stream(std::move(conn));
    co_await stream.readAndParse();
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

    auto conn = co_await net::TcpConnection::connect(addresses[0].toIp().c_str(), parsedUrl.port());

    // Build request line
    std::ostringstream oss;
    oss << method << " " << parsedUrl.path() << " HTTP/1.1\r\n";
    oss << "Host: " << parsedUrl.host() << "\r\n";
    std::string requestLine = oss.str();
    co_await conn->write(requestLine.c_str(), requestLine.size());

    // Create outgoing stream for request body
    HttpOutgoingStream<HttpRequest> requestStream(conn);
    requestStream.setMethod(method);
    requestStream.setPath(parsedUrl.path());

    // Create promise/future for response
    Promise<HttpIncomingStream<HttpResponse>> promise(Scheduler::current());
    auto responseFuture = promise.get_future();

    // Spawn background task to receive response
    Scheduler::current()->spawn([conn, promise = std::move(promise)]() mutable -> Task<> {
        try
        {
            HttpIncomingStream<HttpResponse> response(conn);
            co_await response.readAndParse();

            // Set connection for body streaming
            promise.set_value(std::move(response));
        }
        catch (...)
        {
            promise.set_exception(std::current_exception());
        }
    });

    co_return HttpClientSession{ std::move(requestStream), std::move(responseFuture) };
}

} // namespace nitro_coro::http
