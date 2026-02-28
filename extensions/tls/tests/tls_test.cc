/**
 * @file tls_test.cc
 * @brief Minimal HTTPS server to test TlsStream
 *
 * Generate test cert:
 *   openssl req -x509 -newkey rsa:2048 -keyout key.pem -out cert.pem -days 365 -nodes -subj '/CN=localhost'
 *
 * Test:
 *   curl -k https://localhost:8443/
 */
#include <nitrocoro/core/Scheduler.h>
#include <nitrocoro/net/TcpServer.h>
#include <nitrocoro/tls/TlsContext.h>
#include <nitrocoro/tls/TlsPolicy.h>
#include <nitrocoro/tls/TlsStream.h>
#include <nitrocoro/utils/Debug.h>

#include <cstring>
#include <stdexcept>

using namespace nitrocoro;
using namespace nitrocoro::tls;
using namespace nitrocoro::net;

static const char * kResponse = "HTTP/1.1 200 OK\r\n"
                                "Content-Type: text/html\r\n"
                                "Content-Length: 21\r\n"
                                "Connection: close\r\n"
                                "\r\n"
                                "<h1>Hello, TLS!</h1>\n";

Task<> handleConn(TlsStreamPtr stream)
{
    NITRO_INFO("SNI: %s, ALPN: %s\n", stream->sniName().c_str(), stream->negotiatedAlpn().c_str());
    char buf[4096];
    size_t n = co_await stream->read(buf, sizeof(buf));
    NITRO_INFO("Request: %.*s\n", static_cast<int>(n), buf);
    co_await stream->write(kResponse, strlen(kResponse));
    co_await stream->shutdown();
}

Task<> run(uint16_t port, TlsContextPtr ctx)
{
    TcpServer server(port);
    NITRO_INFO("HTTPS server listening on port %hu\n", port);

    co_await server.start([ctx](TcpConnectionPtr conn) -> Task<> {
        try
        {
            auto stream = co_await TlsStream::accept(conn, ctx);
            co_await handleConn(stream);
        }
        catch (const std::exception & e)
        {
            NITRO_ERROR("TLS error: %s\n", e.what());
        }
    });
}

int main(int argc, char * argv[])
{
    uint16_t port = 8443;
    const char * cert = "cert.pem";
    const char * key = "key.pem";

    if (argc >= 2)
        port = static_cast<uint16_t>(std::stoi(argv[1]));
    if (argc >= 3)
        cert = argv[2];
    if (argc >= 4)
        key = argv[3];

    auto ctx = TlsContext::create(TlsPolicy::defaultServer(cert, key), true);

    Scheduler scheduler;
    scheduler.spawn([port, ctx]() -> Task<> { co_await run(port, ctx); });
    scheduler.run();
}
