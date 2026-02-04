/**
 * @file tcp_test.cc
 * @brief Test program for TcpServer component
 */
#include "TcpServer.h"
#include "CoroScheduler.h"
#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include <cstring>
#include <iostream>

using namespace my_coro;

Task echo_handler(int fd)
{
    char buf[1024];
    while (true)
    {
        auto n = co_await current_scheduler()->async_read(fd, buf, sizeof(buf) - 1);
        if (n <= 0)
        {
            std::cout << "Connection closed: fd=" << fd << "\n";
            break;
        }

        buf[n] = '\0';
        std::cout << "Received from fd(" << fd << ") " << n << " bytes: " << buf << "\n";

        co_await current_scheduler()->async_write(fd, buf, n);
    }
    close(fd);
}

Task tcp_client(int port, const char* message)
{
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    int flags = fcntl(fd, F_GETFL, 0);
    fcntl(fd, F_SETFL, flags | O_NONBLOCK);

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);

    connect(fd, (sockaddr*)&addr, sizeof(addr));
    co_await current_scheduler()->sleep_for(0.1);

    std::cout << "Client sending: " << message << "\n";
    co_await current_scheduler()->async_write(fd, message, strlen(message));

    char buf[1024];
    auto n = co_await current_scheduler()->async_read(fd, buf, sizeof(buf) - 1);
    buf[n] = '\0';
    std::cout << "Client received: " << buf << "\n";

    close(fd);
}

Task tcp_server_main()
{
    TcpServer server(8888);
    server.set_handler(echo_handler);
    co_await server.start();
}

Task tcp_client_main()
{
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    int flags = fcntl(fd, F_GETFL, 0);
    fcntl(fd, F_SETFL, flags | O_NONBLOCK);

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(8888);
    inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);

    connect(fd, (sockaddr*)&addr, sizeof(addr));
    co_await current_scheduler()->sleep_for(0.1);
    std::cout << "Connected to server\n";

    std::string line;
    while (std::getline(std::cin, line))
    {
        if (line == "q") break;
        if (line.empty()) continue;

        co_await current_scheduler()->async_write(fd, line.c_str(), line.size());

        char buf[1024];
        auto n = co_await current_scheduler()->async_read(fd, buf, sizeof(buf) - 1);
        if (n <= 0) break;
        buf[n] = '\0';
        std::cout << "Received: " << buf << "\n";
    }

    close(fd);
    current_scheduler()->stop();
}

int main(int argc, char* argv[])
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
