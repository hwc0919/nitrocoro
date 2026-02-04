/**
 * @file tcp_test.cc
 * @brief Test program for TcpServer component
 */
#include "CoroScheduler.h"
#include "TcpClient.h"
#include "TcpServer.h"
#include "TcpConnection.h"
#include <cstring>
#include <iostream>
#include <netinet/tcp.h>
#include <sys/socket.h>

using namespace my_coro;

#define BUFFER_SIZE 8

Task echo_handler(std::shared_ptr<TcpConnection> conn)
{
    char buf[BUFFER_SIZE];
    while (true)
    {
        ssize_t n;
        co_await conn->read(buf, sizeof(buf) - 1, &n);
        if (n <= 0)
        {
            std::cout << "Connection closed\n";
            break;
        }

        buf[n] = '\0';
        std::cout << "Received " << n << " bytes: " << buf << "\n";
        co_await conn->write(buf, n, &n);
        co_await current_scheduler()->sleep_for(1); // Simulate processing delay
    }
}

Task tcp_server_main()
{
    TcpServer server(8888);
    server.set_handler(echo_handler);
    co_await server.start();
}

Task tcp_client_main()
{
    TcpClient client;
    co_await client.connect("127.0.0.1", 8888);
    std::cout << "Connected to server\n";

    std::string line;
    while (std::getline(std::cin, line))
    {
        if (line == "q")
            break;
        if (line.empty())
            continue;

        ssize_t n;
        co_await client.write(line.c_str(), line.size(), &n);

        size_t total = 0;
        char buf[BUFFER_SIZE];
        while (total < line.size())
        {
            co_await client.read(buf, sizeof(buf) - 1, &n);
            if (n <= 0)
                break;
            buf[n] = '\0';
            fprintf(stdout, "%s", buf);
            fflush(stdout);
            total += n;
        }
        fprintf(stdout, "\n");
        fflush(stdout);
    }

    client.close();
    current_scheduler()->stop();
}

int main(int argc, char * argv[])
{
    if (argc < 2)
    {
        std::cout << "Usage: " << argv[0] << " <server|client> [message]\n";
        return 1;
    }

    CoroScheduler scheduler;

    if (strcmp(argv[1], "server") == 0)
    {
        std::cout << "=== Starting Server ===\n";
        scheduler.spawn(tcp_server_main);
    }
    else if (strcmp(argv[1], "client") == 0)
    {
        std::cout << "=== Starting Client (type 'q' to quit) ===\n";
        scheduler.spawn(tcp_client_main);
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
