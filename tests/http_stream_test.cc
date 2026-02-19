/**
 * @file http_stream_test.cc
 * @brief Test streaming HTTP client with echo server
 */
#include <nitro_coro/core/Scheduler.h>
#include <nitro_coro/http/HttpClient.h>
#include <nitro_coro/http/HttpServer.h>
#include <nitro_coro/utils/Debug.h>

using namespace nitro_coro;
using namespace nitro_coro::http;

Task<> echo_server(uint16_t port)
{
    HttpServer server(port);

    // Echo endpoint - streams back whatever is received
    server.route("POST", "/stream-echo", [](auto & req, auto & resp) -> Task<> {
        NITRO_INFO("[Server] Receive new request\n");
        resp.setStatus(200);
        resp.setHeader("Content-Type", "text/plain");
        auto ctl = req.header(HttpHeader::NameCode::ContentLength);
        resp.setHeader({ HttpHeader::NameCode::ContentLength, std::string{ ctl } });
        NITRO_INFO("[Server] sending headers\n");
        co_await resp.write({});
        NITRO_INFO("[Server] headers sent\n");

        // Stream echo: read and write simultaneously
        while (true)
        {
            auto chunk = co_await req.read(1024);
            if (chunk.empty())
            {
                break;
            }

            NITRO_INFO("[Server] echo chunk: %.*s\n", (int)chunk.size(), chunk.data());
            co_await resp.write(chunk);
        }

        NITRO_INFO("[Server] end response\n");
        co_await resp.end();
    });

    NITRO_INFO("[Server] Listening on port %d\n", port);
    co_await server.start();
}

Task<> test_client(uint16_t port)
{
    // Wait for server to start
    co_await Scheduler::current()->sleep_for(0.1);

    HttpClient client;

    NITRO_INFO("\n=== Test 1: Streaming Upload ===\n");
    auto session = co_await client.stream("POST", "http://127.0.0.1:" + std::to_string(port) + "/stream-echo");

    Promise<> respFinishPromise{ Scheduler::current() };
    Scheduler::current()->spawn([&session, &respFinishPromise]() -> Task<> {
        // Read response
        auto response = co_await session.response.get();
        NITRO_INFO("[Client] Response status: %d\n", response.statusCode());

        while (true)
        {
            try
            {
                auto chunk = co_await response.read(1024);
                if (chunk.empty())
                    break;
                NITRO_INFO("[Client] Recv chunk: %.*s\n", (int)chunk.size(), chunk.data());
            }
            catch (...)
            {
                break;
            }
        }
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

        co_await Scheduler::current()->sleep_for(0.5);
        co_await session.request.write(chunk);
        NITRO_INFO("[Client] Sent chunk: %s\n", chunk.c_str());
        pos += len;
    }

    co_await session.request.end();
    NITRO_INFO("[Client] Request completed\n");

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
