/**
 * @file tcp_chat_test.cc
 * @brief Test program for TCP chat room
 */
#include "Mutex.h"
#include "Scheduler.h"
#include "TcpConnection.h"
#include "TcpServer.h"
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <iostream>
#include <memory>
#include <random>
#include <unistd.h>
#include <unordered_map>

using namespace my_coro;

#define BUFFER_SIZE 1024

struct ChatClient
{
    std::string username;
};

Mutex clientsMutex;
std::unordered_map<std::shared_ptr<TcpConnection>, ChatClient> clients;

Task<> broadcast(const std::string & message, std::shared_ptr<TcpConnection> sender)
{
    printf("broadcast %s\n", message.c_str());

    [[maybe_unused]] auto lock = co_await clientsMutex.scoped_lock();
    for (auto & [conn, client] : clients)
    {
        if (conn != sender)
        {
            printf("broadcast to %s\n", client.username.c_str());
            Scheduler::current()->spawn([message, conn]() -> Task<> {
                static thread_local std::mt19937 gen(std::random_device{}());
                static std::uniform_real_distribution<> dis(0.0, 1.0);
                double delay = dis(gen);

                co_await Scheduler::current()->sleep_for(delay);
                try
                {
                    co_await conn->write(message.c_str(), message.size());
                }
                catch (...)
                {
                }
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
    size_t n = co_await conn->read(buf, sizeof(buf) - 1);
    assert(n > 0);
    buf[n] = '\0';
    username = buf;
    if (!username.empty() && username.back() == '\n')
        username.pop_back();

    {
        [[maybe_unused]] auto lock = co_await clientsMutex.scoped_lock();
        clients[conn] = { username };
    }
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
        assert(n > 0);
        buf[n] = '\0';
        std::string msg = username + ": " + buf;
        std::cout << msg << std::endl;
        co_await broadcast(msg, conn);
    }

    // Remove client
    {
        [[maybe_unused]] auto lock = co_await clientsMutex.scoped_lock();
        auto it = clients.find(conn);
        if (it != clients.end())
        {
            std::cout << username << " left\n";
            clients.erase(it);
        }
    }
}

Task<> tcp_server_main(int port, Scheduler * scheduler)
{
    TcpServer server(scheduler, port);
    co_await server.start(chat_handler);
}

Task<> receive_messages(const TcpConnectionPtr & connPtr)
{
    char buf[BUFFER_SIZE];
    while (true)
    {
        printf("receive message loop\n");
        ssize_t n;
        try
        {
            n = co_await connPtr->read(buf, sizeof(buf) - 1);
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

Task<> send_messages(const TcpConnectionPtr & connPtr)
{
    // Set stdin to non-blocking
    int flags = fcntl(STDIN_FILENO, F_GETFL, 0);
    fcntl(STDIN_FILENO, F_SETFL, flags | O_NONBLOCK);
    std::unique_ptr<IoChannel> stdinChannel = std::make_unique<IoChannel>(STDIN_FILENO, Scheduler::current());
    stdinChannel->enableReading();

    char buf[BUFFER_SIZE];
    std::string line;

    while (true)
    {
        BufferReader reader(buf, sizeof(buf) - 1);
        co_await stdinChannel->performRead(&reader);
        buf[reader.readLen()] = '\0';
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
                co_await connPtr->write(msg.c_str(), msg.size());
            }
        }
    }
}

Task<> tcp_client_main(const char * host, int port, const char * username)
{
    auto connPtr = co_await TcpConnection::connect(host, port);

    // Send username
    std::string user = std::string(username) + "\n";
    co_await connPtr->write(user.c_str(), user.size());

    // Spawn receiver and sender tasks
    Scheduler::current()->spawn([connPtr]() -> Task<> { co_await receive_messages(connPtr); });
    Scheduler::current()->spawn([connPtr]() -> Task<> { co_await send_messages(connPtr); });

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
        std::cout << "  " << argv[0] << " client <username> [port] [host]\n";
        return 1;
    }

    Scheduler scheduler;

    if (strcmp(argv[1], "server") == 0)
    {
        int port = (argc >= 3) ? atoi(argv[2]) : 8888;
        std::cout << "=== Chat Server on port " << port << " ===\n";
        scheduler.spawn([port, &scheduler]() -> Task<> { co_await tcp_server_main(port, &scheduler); });
    }
    else if (strcmp(argv[1], "client") == 0)
    {
        if (argc < 3)
        {
            std::cout << "Usage: " << argv[0] << " client <username> [port] [host]\n";
            return 1;
        }
        const char * username = argv[2];
        int port = (argc >= 4) ? atoi(argv[3]) : 8888;
        const char * host = (argc >= 5) ? argv[4] : "127.0.0.1";
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
