/**
 * @file tcp_chat_server.cc
 * @brief Chat server test program
 */
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <memory>
#include <nitrocoro/core/Mutex.h>
#include <nitrocoro/core/Scheduler.h>
#include <nitrocoro/io/adapters/BufferReader.h>
#include <nitrocoro/net/TcpConnection.h>
#include <nitrocoro/net/TcpServer.h>
#include <nitrocoro/utils/Debug.h>
#include <random>
#include <unistd.h>
#include <unordered_map>

using namespace nitrocoro;
using namespace nitrocoro::net;
using namespace nitrocoro::io;
using nitrocoro::io::adapters::BufferReader;

#define BUFFER_SIZE 1024

struct ChatClient
{
    std::string username;
};

Mutex clientsMutex;
std::unordered_map<std::shared_ptr<TcpConnection>, ChatClient> clients;

Task<> broadcast(const std::string & message, std::shared_ptr<TcpConnection> sender)
{
    NITRO_DEBUG("broadcast %s\n", message.c_str());

    [[maybe_unused]] auto lock = co_await clientsMutex.scoped_lock();
    for (auto & [conn, client] : clients)
    {
        if (conn != sender)
        {
            NITRO_TRACE("broadcast to %s\n", client.username.c_str());
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
}

Task<> chat_handler(std::shared_ptr<TcpConnection> conn)
{
    char buf[BUFFER_SIZE];
    std::string username;

    while (true)
    {
        size_t n;
        try
        {
            n = co_await conn->read(buf, sizeof(buf) - 1);
        }
        catch (const std::exception & e)
        {
            NITRO_ERROR("Read error: %s\n", e.what());
            break;
        }
        if (n == 0)
        {
            NITRO_INFO("User %s disconnect\n", username.c_str());
            break;
        }
        buf[n] = '\0';

        if (strncmp(buf, "quit\n", n) == 0)
        {
            NITRO_INFO("User %s quit\n", username.c_str());
            break;
        }

        if (strncmp(buf, "login ", sizeof("login ") - 1) == 0)
        {
            username = std::string{ buf + sizeof("login ") - 1 };
            if (username.empty() || username == "\n")
            {
                NITRO_DEBUG("Empty username\n");
                continue;
            }
            if (username.back() == '\n')
            {
                username.pop_back();
            }
            NITRO_INFO("User %s joined\n", username.c_str());
            [[maybe_unused]] auto lock = co_await clientsMutex.scoped_lock();
            clients[conn] = { username };
            continue;
        }

        if (username.empty())
        {
            static constexpr char loginTip[] = "Please login first: login <username>";
            co_await conn->write(loginTip, sizeof(loginTip));
            continue;
        }

        std::string msg = username + ": " + buf;
        NITRO_DEBUG("%s: %s\n", username.c_str(), buf);
        co_await broadcast(msg, conn);
    }

    {
        [[maybe_unused]] auto lock = co_await clientsMutex.scoped_lock();
        auto it = clients.find(conn);
        if (it != clients.end())
        {
            NITRO_INFO("User %s left\n", username.c_str());
            clients.erase(it);
        }
    }
}

Task<> server_main(int port, Scheduler * scheduler)
{
    TcpServer * currentServer = nullptr;
    std::atomic_bool running = true;

    // command loop
    scheduler->spawn([&currentServer, &running]() -> Task<> {
        int flags = fcntl(STDIN_FILENO, F_GETFL, 0);
        fcntl(STDIN_FILENO, F_SETFL, flags | O_NONBLOCK);
        auto stdinChannel = std::make_unique<IoChannel>(STDIN_FILENO);
        stdinChannel->enableReading();

        char buf[BUFFER_SIZE];
        std::string line;

        while (running.load())
        {
            BufferReader reader(buf, sizeof(buf) - 1);
            auto result = co_await stdinChannel->performRead(&reader);
            if (result != IoChannel::IoResult::Success)
                break;
            buf[reader.readLen()] = '\0';
            line += buf;

            size_t pos;
            while ((pos = line.find('\n')) != std::string::npos)
            {
                std::string msg = line.substr(0, pos);
                line.erase(0, pos + 1);
                if (msg == "restart")
                {
                    if (currentServer)
                        co_await currentServer->stop();
                }
                else if (msg == "quit")
                {
                    running = false;
                    if (currentServer)
                        co_await currentServer->stop();
                }
                else
                {
                    co_await broadcast("system: " + msg + "\n", nullptr);
                }
            }
        }
    });

    while (running.load())
    {
        TcpServer server(port, scheduler);
        currentServer = &server;
        co_await server.start(chat_handler);
    }

    scheduler->stop();
}

int main(int argc, char * argv[])
{
    int port = (argc >= 2) ? atoi(argv[1]) : 8888;
    NITRO_INFO("=== Chat Server on port %d ===\n", port);

    Scheduler scheduler;
    scheduler.spawn([port, &scheduler]() -> Task<> { co_await server_main(port, &scheduler); });
    scheduler.run();

    NITRO_INFO("=== Done ===\n");
    return 0;
}
