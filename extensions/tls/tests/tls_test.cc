/**
 * @file tls_test.cc
 * @brief Minimal HTTPS server to test TlsConnection
 *
 * Generate test cert:
 *   openssl req -x509 -newkey rsa:2048 -keyout key.pem -out cert.pem -days 365 -nodes -subj '/CN=localhost'
 *
 * Test:
 *   curl -k https://localhost:8443/
 */
#include <nitrocoro/core/Scheduler.h>
#include <nitrocoro/io/IoChannel.h>
#include <nitrocoro/tls/TlsConnection.h>
#include <nitrocoro/utils/Debug.h>

#include <fcntl.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include <cstring>
#include <stdexcept>

using namespace nitrocoro;
using namespace nitrocoro::tls;
using namespace nitrocoro::io;

static const char * kResponse =
    "HTTP/1.1 200 OK\r\n"
    "Content-Type: text/html\r\n"
    "Content-Length: 21\r\n"
    "Connection: close\r\n"
    "\r\n"
    "<h1>Hello, TLS!</h1>\n";

Task<> handleConn(TlsConnectionPtr conn)
{
    char buf[4096];
    co_await conn->read(buf, sizeof(buf));
    co_await conn->write(kResponse, strlen(kResponse));
    co_await conn->close();
}

Task<> run(uint16_t port, TlsContextPtr ctx)
{
    int listenFd = socket(AF_INET, SOCK_STREAM, 0);
    int opt = 1;
    setsockopt(listenFd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);
    bind(listenFd, reinterpret_cast<sockaddr *>(&addr), sizeof(addr));

    int flags = fcntl(listenFd, F_GETFL, 0);
    fcntl(listenFd, F_SETFL, flags | O_NONBLOCK);
    listen(listenFd, 128);

    NITRO_INFO("HTTPS server listening on port %hu\n", port);

    auto listenChannel = std::make_unique<IoChannel>(listenFd, TriggerMode::LevelTriggered);
    listenChannel->enableReading();

    while (true)
    {
        int clientFd = -1;
        co_await listenChannel->performRead([&clientFd](int fd, IoChannel *) -> IoChannel::IoResult {
            clientFd = ::accept4(fd, nullptr, nullptr, SOCK_NONBLOCK | SOCK_CLOEXEC);
            if (clientFd >= 0)
                return IoChannel::IoResult::Success;
            if (errno == EAGAIN || errno == EWOULDBLOCK)
                return IoChannel::IoResult::WouldBlock;
            return IoChannel::IoResult::Error;
        });

        NITRO_DEBUG("Accepted fd=%d\n", clientFd);
        Scheduler::current()->spawn([clientFd, ctx]() -> Task<> {
            try
            {
                auto conn = co_await TlsConnection::accept(clientFd, ctx);
                co_await handleConn(conn);
            }
            catch (const std::exception & e)
            {
                NITRO_ERROR("TLS error: %s\n", e.what());
                ::close(clientFd);
            }
        });
    }
}

int main(int argc, char * argv[])
{
    uint16_t port = 8443;
    const char * cert = "cert.pem";
    const char * key = "key.pem";

    if (argc >= 2) port = static_cast<uint16_t>(std::stoi(argv[1]));
    if (argc >= 3) cert = argv[2];
    if (argc >= 4) key = argv[3];

    auto ctx = TlsContext::forServer(cert, key);

    Scheduler scheduler;
    scheduler.spawn([port, ctx]() -> Task<> { co_await run(port, ctx); });
    scheduler.run();
}
