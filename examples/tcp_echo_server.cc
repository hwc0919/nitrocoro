/**
 * @file tcp_echo_server.cc
 * @brief Echo server test program
 */
#include <cstdlib>
#include <nitrocoro/core/Scheduler.h>
#include <nitrocoro/net/TcpConnection.h>
#include <nitrocoro/net/TcpServer.h>
#include <nitrocoro/utils/Debug.h>

using namespace nitrocoro;
using namespace nitrocoro::net;

#define BUFFER_SIZE 8

Task<> echo_handler(std::shared_ptr<TcpConnection> conn)
{
    char buf[BUFFER_SIZE];
    while (true)
    {
        size_t n = co_await conn->read(buf, sizeof(buf) - 1);
        if (n == 0)
        {
            NITRO_INFO("Connection closed\n");
            break;
        }

        buf[n] = '\0';
        NITRO_INFO("Received %zu bytes: %s\n", n, buf);
        try
        {
            co_await conn->write(buf, n);
        }
        catch (const std::exception & ex)
        {
            NITRO_ERROR("Write error: %s\n", ex.what());
            break;
        }
    }
}

int main(int argc, char * argv[])
{
    int port = (argc >= 2) ? atoi(argv[1]) : 8888;
    NITRO_INFO("=== Echo Server on port %d ===\n", port);

    Scheduler scheduler;
    TcpServer server(port, &scheduler);
    scheduler.spawn([&server]() -> Task<> { co_await server.start(echo_handler); });
    scheduler.run();

    NITRO_INFO("=== Done ===\n");
    return 0;
}
