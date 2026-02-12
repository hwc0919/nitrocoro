/**
 * @file tcp_chat_test.cc
 * @brief Test program for TCP chat room
 */
#include "Scheduler.h"
#include "TcpClient.h"
#include "TcpConnection.h"
#include "TcpServer.h"
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <iostream>
#include <memory>
#include <random>
#include <unistd.h>
#include <vector>

using namespace my_coro;

#define BUFFER_SIZE 1024

struct ChatClient
{
    std::shared_ptr<TcpConnection> conn;
    std::string username;
};

std::vector<ChatClient> clients;

Task<> broadcast(const std::string & message, std::shared_ptr<TcpConnection> sender)
{
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_real_distribution<> dis(0.0, 1.0);

    printf("broadcast %s\n", message.c_str());
    for (auto & client : clients)
    {
        if (client.conn != sender)
        {
            printf("broadcast to %s\n", client.username.c_str());
            double delay = dis(gen);
            Scheduler::current()->spawn([message, conn = client.conn, delay]() -> Task<> {
                co_await Scheduler::current()->sleep_for(delay);
                co_await conn->write(message.c_str(), message.size());
            });
        }
    }
    co_return;
}

Task<> chat_handler(std::shared_ptr<TcpConnection> conn)
{
    char buf[BUFFER_SIZE];
    std::string username;

    // Read username
    ssize_t n = co_await conn->read(buf, sizeof(buf) - 1);
    if (n <= 0)
        co_return;
    buf[n] = '\0';
    username = buf;
    if (!username.empty() && username.back() == '\n')
        username.pop_back();

    clients.push_back({ conn, username });
    std::cout << username << " joined\n";

    // Broadcast messages
    while (true)
    {
        try
        {
            n = co_await conn->read(buf, sizeof(buf) - 1);
        }
        catch (const std::exception & e)
        {
            printf("Read error: %s\n", e.what());
            break;
        }
        buf[n] = '\0';
        std::string msg = username + ": " + buf;
        std::cout << msg << std::endl;
        co_await broadcast(msg, conn);
    }

    // Remove client
    for (auto it = clients.begin(); it != clients.end(); ++it)
    {
        if (it->conn == conn)
        {
            std::cout << username << " left\n";
            clients.erase(it);
            break;
        }
    }
}

Task<> tcp_server_main(int port)
{
    TcpServer server(port);
    server.set_handler(chat_handler);
    co_await server.start();
}

Task<> receive_messages(TcpClient & client)
{
    char buf[BUFFER_SIZE];
    while (true)
    {
        printf("receive message loop\n");
        ssize_t n;
        try
        {
            n = co_await client.read(buf, sizeof(buf) - 1);
        }
        catch (const std::exception & e)
        {
            printf("Read error: %s\n", e.what());
            Scheduler::current()->stop();
            break;
        }
        buf[n] = '\0';
        std::cout << buf;
        std::cout.flush();
    }
}

Task<> send_messages(TcpClient & client, IoChannel * stdinChannel)
{
    char buf[BUFFER_SIZE];
    std::string line;

    while (true)
    {
        ssize_t n = co_await stdinChannel->read(buf, sizeof(buf) - 1);
        if (n <= 0)
            break;

        buf[n] = '\0';
        line += buf;

        // Process complete lines
        size_t pos;
        while ((pos = line.find('\n')) != std::string::npos)
        {
            std::string msg = line.substr(0, pos);
            line.erase(0, pos + 1);

            if (msg == "q")
            {
                Scheduler::current()->stop();
                co_return;
            }
            if (!msg.empty())
            {
                msg += "\n";
                co_await client.write(msg.c_str(), msg.size());
            }
        }
    }
}

Task<> tcp_client_main(const char * host, int port, const char * username)
{
    TcpClient client;
    co_await client.connect(host, port);

    // Send username
    std::string user = std::string(username) + "\n";
    co_await client.write(user.c_str(), user.size());

    // Set stdin to non-blocking
    int flags = fcntl(STDIN_FILENO, F_GETFL, 0);
    fcntl(STDIN_FILENO, F_SETFL, flags | O_NONBLOCK);
    std::unique_ptr<IoChannel> stdinChannel = std::make_unique<IoChannel>(STDIN_FILENO, Scheduler::current());

    // Spawn receiver and sender tasks
    Scheduler::current()->spawn([&client]() -> Task<> { co_await receive_messages(client); });
    Scheduler::current()->spawn([&client, stdinPtr = stdinChannel.get()]() -> Task<> { co_await send_messages(client, stdinPtr); });

    // Keep running until stopped
    while (true)
    {
        co_await Scheduler::current()->sleep_for(1.0);
    }
}

int main(int argc, char * argv[])
{
    if (argc < 2)
    {
        std::cout << "Usage:\n";
        std::cout << "  " << argv[0] << " server [port]\n";
        std::cout << "  " << argv[0] << " client [host] [port] [username]\n";
        return 1;
    }

    Scheduler scheduler;

    if (strcmp(argv[1], "server") == 0)
    {
        int port = (argc >= 3) ? atoi(argv[2]) : 8888;
        std::cout << "=== Chat Server on port " << port << " ===\n";
        scheduler.spawn([port]() -> Task<> { co_await tcp_server_main(port); });
    }
    else if (strcmp(argv[1], "client") == 0)
    {
        const char * host = (argc >= 3) ? argv[2] : "127.0.0.1";
        int port = (argc >= 4) ? atoi(argv[3]) : 8888;
        const char * username = (argc >= 5) ? argv[4] : "User";
        std::cout << "=== Chat Client (" << username << ") ===\n";
        std::cout << "Type 'q' to quit\n";
        scheduler.spawn([host, port, username]() -> Task<> { co_await tcp_client_main(host, port, username); });
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
