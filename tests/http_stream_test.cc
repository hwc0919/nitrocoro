/**
 * @file http_stream_test.cc
 * @brief Test streaming HTTP client with echo server
 */
#include <iostream>
#include <nitro_coro/core/Scheduler.h>
#include <nitro_coro/http/HttpClient.h>
#include <nitro_coro/http/HttpServer.h>

using namespace nitro_coro;
using namespace nitro_coro::http;

Task<> echo_server(uint16_t port)
{
    HttpServer server(port);

    // Echo endpoint - streams back whatever is received
    server.route("POST", "/stream-echo", [](auto & req, auto & resp) -> Task<> {
        std::cout << "[Server] Receive new request" << std::endl;
        resp.setStatus(200);
        resp.setHeader("Content-Type", "text/plain");
        co_await resp.write({});

        // Stream echo: read and write simultaneously
        while (true)
        {
            auto chunk = co_await req.read(1024);
            if (chunk.empty())
            {
                break;
            }

            std::cout << "[Server] chunk: " << chunk << std::endl;
            co_await resp.write(chunk);
        }

        std::cout << "[Server] end response" << std::endl;
        co_await resp.end();
    });

    std::cout << "[Server] Listening on port " << port << "\n";
    co_await server.start();
}

Task<> test_client(uint16_t port)
{
    // Wait for server to start
    co_await Scheduler::current()->sleep_for(0.1);

    HttpClient client;

    std::cout << "\n=== Test 1: Streaming Upload ===\n";
    auto session = co_await client.stream("POST", "http://127.0.0.1:" + std::to_string(port) + "/stream-echo");

    Promise<> respFinishPromise{ Scheduler::current() };
    Scheduler::current()->spawn([&session, &respFinishPromise]() -> Task<> {
        // Read response
        auto response = co_await session.response.get();
        std::cout << "[Client] Response status: " << response.statusCode() << "\n";
        std::cout << "[Client] Response body: ";

        while (true)
        {
            auto chunk = co_await response.read(1024);
            if (chunk.empty())
                break;
            std::cout << chunk;
        }
        std::cout << "\n";
        respFinishPromise.set_value();
    });

    // Set Content-Length before sending body
    std::string data = "Hello World from streaming client!";
    session.request.setHeader("Content-Length", std::to_string(data.size()));

    // Send data in chunks
    size_t pos = 0;
    size_t chunkSize = 6;
    while (pos < data.size())
    {
        size_t len = std::min(chunkSize, data.size() - pos);
        std::string chunk = data.substr(pos, len);
        co_await session.request.write(chunk);
        std::cout << "[Client] Sent: " << chunk << "\n";
        pos += len;
    }

    co_await session.request.end();
    std::cout << "[Client] Request completed\n";

    co_await respFinishPromise.get_future().get();
    Scheduler::current()->stop();
}

int main()
{
    uint16_t port = 9999;

    Scheduler scheduler;

    // Start server
    scheduler.spawn([port]() -> Task<> {
        co_await echo_server(port);
    });

    // Start client
    scheduler.spawn([port]() -> Task<> {
        co_await test_client(port);
    });

    scheduler.run();

    return 0;
}
