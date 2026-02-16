/**
 * @file tcp_test.cc
 * @brief Test program for TcpServer component
 */
#include <nitro_coro/core/Scheduler.h>
#include <nitro_coro/net/TcpConnection.h>
#include <nitro_coro/net/TcpServer.h>
#include <cstdlib>
#include <cstring>
#include <iostream>

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
        co_await conn->write(buf, n);
        co_await Scheduler::current()->sleep_for(1); // Simulate processing delay
    }
}

Task<> tcp_server_main(int port, Scheduler * scheduler)
{
    TcpServer server(scheduler, port);
    co_await server.start(echo_handler);
}

Task<> tcp_client_main(const char * host, int port)
{
    auto connPtr = co_await TcpConnection::connect(host, port);
    std::cout << "Connected to server\n";

    std::string line;
    while (std::getline(std::cin, line))
    {
        if (line == "q")
            break;
        if (line.empty())
            continue;

        co_await connPtr->write(line.c_str(), line.size());
        co_await connPtr->write("\n", 1);

        // Read until newline
        std::string response;
        char buf[BUFFER_SIZE];
        while (true)
        {
            ssize_t n = co_await connPtr->read(buf, sizeof(buf) - 1);
            if (n <= 0)
                break;
            buf[n] = '\0';
            response += buf;
            if (response.find('\n') != std::string::npos)
                break;
        }
        std::cout << response;
        std::cout.flush();
    }

    co_await connPtr->close();
    Scheduler::current()->stop();
}

int main(int argc, char * argv[])
{
    if (argc < 2)
    {
        std::cout << "Usage:\n";
        std::cout << "  " << argv[0] << " server [port]\n";
        std::cout << "  " << argv[0] << " client [host] [port]\n";
        return 1;
    }

    Scheduler scheduler;

    if (strcmp(argv[1], "server") == 0)
    {
        int port = (argc >= 3) ? atoi(argv[2]) : 8888;
        std::cout << "=== Starting Server on port " << port << " ===\n";
        scheduler.spawn([port, &scheduler]() -> Task<> { co_await tcp_server_main(port, &scheduler); });
    }
    else if (strcmp(argv[1], "client") == 0)
    {
        const char * host = (argc >= 3) ? argv[2] : "127.0.0.1";
        int port = (argc >= 4) ? atoi(argv[3]) : 8888;
        std::cout << "=== Starting Client (connecting to " << host << ":" << port << ") ===\n";
        std::cout << "Type 'q' to quit\n";
        scheduler.spawn([host, port]() -> Task<> { co_await tcp_client_main(host, port); });
    }
    else
    {
        std::cout << "Invalid mode. Use 'server' or 'client'\n";
        return 1;
    }

    scheduler.run();
    std::cout << "=== Done ===\n";
    return 0;
}
