/**
 * @file tcp_chat_test.cc
 * @brief Test program for TCP chat room
 */
#include "CoroScheduler.h"
#include "TcpClient.h"
#include "TcpConnection.h"
#include "TcpServer.h"
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <memory>
#include <vector>

using namespace my_coro;

#define BUFFER_SIZE 256

struct ChatClient
{
    std::shared_ptr<TcpConnection> conn;
    std::string username;
};

std::vector<ChatClient> clients;

Task<> broadcast(const std::string & message, std::shared_ptr<TcpConnection> sender)
{
    for (auto & client : clients)
    {
        if (client.conn != sender)
        {
            co_await client.conn->write(message.c_str(), message.size());
        }
    }
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
        n = co_await conn->read(buf, sizeof(buf) - 1);
        if (n <= 0)
            break;
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
        ssize_t n = co_await client.read(buf, sizeof(buf) - 1);
        if (n <= 0)
        {
            std::cout << "\nDisconnected from server\n";
            CoroScheduler::current()->stop();
            break;
        }
        buf[n] = '\0';
        std::cout << buf;
        std::cout.flush();
    }
}

Task<> tcp_client_main(const char * host, int port, const char * username)
{
    TcpClient client;
    co_await client.connect(host, port);

    // Send username
    std::string user = std::string(username) + "\n";
    co_await client.write(user.c_str(), user.size());

    // Spawn receiver task
    CoroScheduler::current()->spawn([&client]() -> Task<> { co_await receive_messages(client); });

    // Send messages
    std::string line;
    while (std::getline(std::cin, line))
    {
        if (line == "q")
            break;
        if (line.empty())
            continue;
        line += "\n";
        co_await client.write(line.c_str(), line.size());
    }

    client.close();
    CoroScheduler::current()->stop();
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

    CoroScheduler scheduler;

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
