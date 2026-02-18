/**
 * @file tcp_client.cc
 * @brief Generic TCP client
 */
#include <cstdlib>
#include <fcntl.h>
#include <iostream>
#include <nitro_coro/core/Future.h>
#include <nitro_coro/core/Scheduler.h>
#include <nitro_coro/io/IoChannel.h>
#include <nitro_coro/io/adapters/BufferReader.h>
#include <nitro_coro/net/TcpConnection.h>
#include <unistd.h>

using namespace nitro_coro;
using namespace nitro_coro::net;
using namespace nitro_coro::io;
using nitro_coro::io::adapters::BufferReader;

#define BUFFER_SIZE 1024

Task<> receive_messages(const TcpConnectionPtr & connPtr, Promise<> & closePromise)
{
    char buf[BUFFER_SIZE];
    while (true)
    {
        try
        {
            ssize_t n = co_await connPtr->read(buf, sizeof(buf) - 1);
            buf[n] = '\0';
            std::cout << buf;
            std::cout.flush();
        }
        catch (const std::exception & e)
        {
            closePromise.set_value();
            break;
        }
    }
}

Task<> send_messages(const TcpConnectionPtr & connPtr, Promise<> & closePromise)
{
    int flags = fcntl(STDIN_FILENO, F_GETFL, 0);
    fcntl(STDIN_FILENO, F_SETFL, flags | O_NONBLOCK);
    auto stdinChannel = IoChannel::create(STDIN_FILENO, Scheduler::current());
    stdinChannel->enableReading();

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
                closePromise.set_value();
                co_return;
            }
            co_await connPtr->write(msg.c_str(), msg.size());
        }
    }
}

Task<> client_main(const char * host, int port)
{
    auto connPtr = co_await TcpConnection::connect(host, port);
    printf("Connected to %s:%hu\n", host, port);

    Promise<> closePromise(Scheduler::current());
    auto closeFuture = closePromise.get_future();

    Scheduler::current()->spawn([connPtr, &closePromise]() -> Task<> { co_await receive_messages(connPtr, closePromise); });
    Scheduler::current()->spawn([connPtr, &closePromise]() -> Task<> { co_await send_messages(connPtr, closePromise); });

    co_await closeFuture.get();
    Scheduler::current()->stop();
}

int main(int argc, char * argv[])
{
    int port = (argc >= 2) ? atoi(argv[1]) : 8888;
    const char * host = (argc >= 3) ? argv[2] : "127.0.0.1";

    std::cout << "=== TCP Client ===\n";
    std::cout << "Type 'q' to quit\n";

    Scheduler scheduler;
    scheduler.spawn([host, port]() -> Task<> { co_await client_main(host, port); });
    scheduler.run();

    std::cout << "=== Done ===\n";
    return 0;
}
