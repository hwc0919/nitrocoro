/**
 * @file WsServer.cc
 * @brief WebSocket upgrade handshake + routing
 */
#include <nitrocoro/websocket/WsServer.h>

#include <nitrocoro/http/HttpHeader.h>
#include <nitrocoro/utils/Base64.h>
#include <nitrocoro/utils/Sha1.h>

#include <string>

static std::string computeAccept(const std::string & key)
{
    static constexpr std::string_view kGuid = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
    auto digest = nitrocoro::utils::sha1(key + std::string(kGuid));
    return nitrocoro::utils::base64Encode(std::string_view(reinterpret_cast<const char *>(digest.data()), digest.size()));
}

namespace nitrocoro::websocket
{

void WsServer::route(const std::string & path, Handler handler)
{
    routes_[path] = std::move(handler);
}

void WsServer::attachTo(http::HttpServer & server)
{
    server.setRequestUpgrader([this](http::HttpIncomingStream<http::HttpRequest> & req, io::StreamPtr stream) -> Task<bool> {
        co_return co_await handleUpgrade(req, std::move(stream));
    });
}

Task<bool> WsServer::handleUpgrade(http::HttpIncomingStream<http::HttpRequest> & req, io::StreamPtr stream)
{
    using http::HttpHeader;

    // Only handle WebSocket upgrades
    auto & upgrade = req.getHeader(HttpHeader::Name::Upgrade_L);
    if (upgrade != "websocket")
        co_return false;

    auto it = routes_.find(std::string(req.path()));
    if (it == routes_.end())
        co_return false;

    auto & key = req.getHeader("sec-websocket-key");
    if (key.empty())
        co_return false;

    std::string accept = computeAccept(key);

    // Send 101 Switching Protocols directly on the stream
    std::string response = "HTTP/1.1 101 Switching Protocols\r\n"
                           "Upgrade: websocket\r\n"
                           "Connection: Upgrade\r\n"
                           "Sec-WebSocket-Accept: "
                           + accept + "\r\n"
                                      "\r\n";
    co_await stream->write(response.data(), response.size());

    WsConnection conn(std::move(stream));
    co_await it->second(conn);
    co_return true;
}

} // namespace nitrocoro::websocket
