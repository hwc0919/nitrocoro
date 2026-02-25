/**
 * @file tcp_client.cc
 * @brief Generic TCP client
 */
#include <cstdlib>
#include <fcntl.h>
#include <nitrocoro/core/Future.h>
#include <nitrocoro/core/Scheduler.h>
#include <nitrocoro/io/IoChannel.h>
#include <nitrocoro/io/adapters/BufferReader.h>
#include <nitrocoro/net/Dns.h>
#include <nitrocoro/net/TcpConnection.h>
#include <nitrocoro/utils/Debug.h>
#include <unistd.h>

using namespace nitrocoro;
using namespace nitrocoro::net;
using namespace nitrocoro::io;
using nitrocoro::io::adapters::BufferReader;

#define BUFFER_SIZE 1024

Task<> receive_messages(const TcpConnectionPtr & connPtr)
{
    char buf[BUFFER_SIZE];
    while (true)
    {
        try
        {
            ssize_t n = co_await connPtr->read(buf, sizeof(buf) - 1);
            buf[n] = '\0';
            printf("%s", buf);
            fflush(stdout);
        }
        catch (const std::exception & e)
        {
            break;
        }
    }
}

static std::shared_ptr<IoChannel> getStdinChannel()
{
    static auto channel = []() {
        int flags = fcntl(STDIN_FILENO, F_GETFL, 0);
        fcntl(STDIN_FILENO, F_SETFL, flags | O_NONBLOCK);
        auto stdinChannel = std::make_shared<IoChannel>(STDIN_FILENO);
        stdinChannel->enableReading();
        return stdinChannel;
    }();
    return channel;
}

Task<> send_messages(const TcpConnectionPtr & connPtr)
{
    auto stdinChannel = getStdinChannel();

    char buf[BUFFER_SIZE];
    std::string line;

    while (true)
    {
        BufferReader reader(buf, sizeof(buf) - 1);
        co_await stdinChannel->performRead(&reader);
        buf[reader.readLen()] = '\0';
        line += buf;

        size_t pos;
        while ((pos = line.find('\n')) != std::string::npos)
        {
            std::string msg = line.substr(0, pos + 1);
            line.erase(0, pos + 1);

            if (msg == "q\n")
            {
                co_return;
            }
            co_await connPtr->write(msg.c_str(), msg.size());
        }
    }
}

Task<> client_main(const char * host, int port)
{
    bool quit{ false };
    while (!quit)
    {
        // Resolve hostname
        NITRO_INFO("Resolving %s...\n", host);
        auto addresses = co_await net::resolve(host);
        if (addresses.empty())
        {
            NITRO_ERROR("Failed to resolve %s\n", host);
            co_return;
        }
        NITRO_INFO("Resolved to %s\n", addresses[0].toIp().c_str());

        // Connect using resolved IP
        auto connPtr = co_await TcpConnection::connect(addresses[0].toIp().c_str(), port);
        NITRO_INFO("Connected to %s:%hu\n", host, port);

        Promise<> closePromise(Scheduler::current());
        auto closeFuture = closePromise.get_future();

        Scheduler::current()->spawn([connPtr, &closePromise]() -> Task<> {
            co_await receive_messages(connPtr);
            closePromise.set_value();
        });
        Scheduler::current()->spawn([connPtr, &quit]() -> Task<> {
            co_await send_messages(connPtr);
            co_await connPtr->close();
            quit = true;
        });
        co_await closeFuture.get();
    }

    Scheduler::current()->stop();
}

int main(int argc, char * argv[])
{
    int port = (argc >= 2) ? atoi(argv[1]) : 8888;
    const char * host = (argc >= 3) ? argv[2] : "127.0.0.1";

    printf("=== TCP Client ===\n");
    printf("Type 'q' to quit\n");

    Scheduler scheduler;
    scheduler.spawn([host, port]() -> Task<> { co_await client_main(host, port); });
    scheduler.run();

    printf("=== Done ===\n");
    return 0;
}
