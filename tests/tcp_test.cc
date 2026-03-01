/**
 * @file tcp_test.cc
 * @brief Tests for TcpServer and TcpConnection.
 */
#include <nitrocoro/core/Future.h>
#include <nitrocoro/core/Scheduler.h>
#include <nitrocoro/core/Task.h>
#include <nitrocoro/net/InetAddress.h>
#include <nitrocoro/net/TcpConnection.h>
#include <nitrocoro/net/TcpServer.h>
#include <nitrocoro/testing/Test.h>

using namespace nitrocoro;
using namespace nitrocoro::net;

/** Client sends data; server echoes it back; client verifies the round-trip. */
NITRO_TEST(tcp_echo)
{
    TcpServer server(0);
    uint16_t port = server.port();

    Scheduler::current()->spawn([TEST_CTX, &server]() -> Task<> {
        co_await server.start([](TcpConnectionPtr conn) -> Task<> {
            char buf[256];
            size_t n = co_await conn->read(buf, sizeof(buf));
            co_await conn->write(buf, n);
        });
    });

    co_await Scheduler::current()->sleep_for(0.01);

    auto conn = co_await TcpConnection::connect({"127.0.0.1", port});
    co_await conn->write("hello", 5);

    char buf[256]{};
    size_t n = co_await conn->read(buf, sizeof(buf));
    NITRO_CHECK_EQ(n, 5u);
    NITRO_CHECK(std::string_view(buf, n) == "hello");

    co_await server.stop();
}

/** stop() causes start() to return and the server shuts down cleanly. */
NITRO_TEST(tcp_server_stop)
{
    TcpServer server(0);
    bool started = false;
    bool stopped = false;

    Scheduler::current()->spawn([TEST_CTX, &server, &started, &stopped]() -> Task<> {
        started = true;
        co_await server.start([](TcpConnectionPtr) -> Task<> { co_return; });
        stopped = true;
    });

    co_await Scheduler::current()->sleep_for(0.01);
    NITRO_CHECK(started);

    co_await server.stop();
    co_await Scheduler::current()->sleep_for(0.01);
    NITRO_CHECK(stopped);
}

/** Multiple clients connect concurrently and each receives its own echo. */
NITRO_TEST(tcp_multiple_clients)
{
    TcpServer server(0);
    uint16_t port = server.port();

    Scheduler::current()->spawn([TEST_CTX, &server]() -> Task<> {
        auto somePtr = std::make_shared<int>(0);
        NITRO_CHECK(somePtr.use_count() == 1);

        co_await server.start([somePtr](TcpConnectionPtr conn) -> Task<> {
            NITRO_INFO("somePtr.use_count = %zu\n", somePtr.use_count());
            ++(*somePtr);
            char buf[256];
            size_t n = co_await conn->read(buf, sizeof(buf));
            co_await conn->write(buf, n);
        });

        // server.start() block until server.stop() is called

        /** GCC versions before 13 have a known bug: shared_ptr captured in a coroutine lambda
         *  may have its lifetime incorrectly managed, causing use_count to be invalid here. */
        NITRO_INFO("somePtr.use_count = %zu, should be a valid number\n", somePtr.use_count());
        NITRO_CHECK(somePtr.use_count() == 1);
        ++(*somePtr); // might crash
    });

    co_await Scheduler::current()->sleep_for(0.01);

    constexpr int kClients = 5;
    int received = 0;
    Promise<> done(Scheduler::current());
    auto f = done.get_future();

    for (int i = 0; i < kClients; ++i)
    {
        Scheduler::current()->spawn([TEST_CTX, port, i, &received, &done]() mutable -> Task<> {
            auto conn = co_await TcpConnection::connect({"127.0.0.1", port});
            std::string msg = "client" + std::to_string(i);
            co_await conn->write(msg.data(), msg.size());

            char buf[256]{};
            size_t n = co_await conn->read(buf, sizeof(buf));
            NITRO_CHECK(std::string_view(buf, n) == msg);

            if (++received == kClients)
                done.set_value();
        });
    }

    co_await f.get();
    co_await server.stop();
}

int main()
{
    return nitrocoro::test::run_all();
}
