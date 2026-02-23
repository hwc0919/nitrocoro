/**
 * @file tcp_echo_server.cc
 * @brief Echo server test program
 */
#include <cstdlib>
#include <iostream>
#include <nitro_coro/core/Scheduler.h>
#include <nitro_coro/net/TcpConnection.h>
#include <nitro_coro/net/TcpServer.h>

using namespace nitro_coro;
using namespace nitro_coro::net;

#define BUFFER_SIZE 8

Task<> echo_handler(std::shared_ptr<TcpConnection> conn)
{
    char buf[BUFFER_SIZE];
    while (true)
    {
        ssize_t n = co_await conn->read(buf, sizeof(buf) - 1);
        if (n <= 0)
        {
            std::cout << "Connection closed\n";
            break;
        }

        buf[n] = '\0';
        std::cout << "Received " << n << " bytes: " << buf << "\n";
        try
        {
            co_await conn->write(buf, n);
        }
        catch (const std::exception & ex)
        {
            std::cout << "Write error: " << ex.what() << "\n";
            break;
        }
    }
}

int main(int argc, char * argv[])
{
    int port = (argc >= 2) ? atoi(argv[1]) : 8888;
    std::cout << "=== Echo Server on port " << port << " ===\n";

    Scheduler scheduler;
    TcpServer server(port, &scheduler);
    scheduler.spawn([&server]() -> Task<> { co_await server.start(echo_handler); });
    scheduler.run();

    std::cout << "=== Done ===\n";
    return 0;
}
