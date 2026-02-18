/**
 * @file http_server.cc
 * @brief Simple HTTP server test
 */
#include <nitro_coro/core/Scheduler.h>
#include <nitro_coro/http/HttpServer.h>

using namespace nitro_coro;
using namespace nitro_coro::http;

Task<> server_main(uint16_t port)
{
    HttpServer server(port);

    server.route("GET", "/", [](HttpRequest & req, HttpResponse & resp) -> Task<> {
        resp.setStatus(200);
        resp.setHeader("Content-Type", "text/html");
        co_await resp.write("<h1>Hello, World!</h1>");
    });

    server.route("GET", "/hello", [](HttpRequest & req, HttpResponse & resp) -> Task<> {
        auto name = req.query("name");
        std::string body = "Hello, ";
        body += name.empty() ? "Guest" : name;
        body += "!";

        resp.setStatus(200);
        resp.setHeader("Content-Type", "text/plain");
        co_await resp.write(body);
    });

    server.route("POST", "/echo", [](HttpRequest & req, HttpResponse & resp) -> Task<> {
        resp.setStatus(200);
        resp.setHeader("Content-Type", "text/plain");
        co_await resp.write(req.body());
    });

    co_await server.start();
}

int main(int argc, char * argv[])
{
    uint16_t port = (argc >= 2) ? atoi(argv[1]) : 8080;

    printf("=== HTTP Server Test ===\n"
           "Try:\n"
           "  curl http://localhost:%hu/\n"
           "  curl http://localhost:%hu/hello?name=Alice\n"
           "  curl -X POST -d 'test data' http://localhost:%hu/echo\n",
           port, port, port);

    Scheduler scheduler;
    scheduler.spawn([port]() -> Task<> { co_await server_main(port); });
    scheduler.run();

    return 0;
}
