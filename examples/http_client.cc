/**
 * @file http_client.cc
 * @brief HTTP client test
 */
#include <nitro_coro/core/Scheduler.h>
#include <nitro_coro/http/HttpClient.h>

using namespace nitro_coro;
using namespace nitro_coro::http;

Task<> client_main(const char * url)
{
    HttpClient client;

    printf("GET %s\n", url);
    try
    {
        auto resp = co_await client.get(url);
        printf("Status: %d %s\n", (int)resp.statusCode(), resp.statusReason().data());
        printf("Body:\n%s\n", resp.body().c_str());
    }
    catch (const std::exception & e)
    {
        printf("Error: %s\n", e.what());
    }
}

int main(int argc, char * argv[])
{
    if (argc < 2)
    {
        printf("Usage: %s <url>\n", argv[0]);
        printf("Example: %s http://localhost:8080/\n", argv[0]);
        return 1;
    }

    Scheduler scheduler;
    scheduler.spawn([url = argv[1], &scheduler]() -> Task<> {
        co_await client_main(url);
        scheduler.stop();
    });
    scheduler.run();

    return 0;
}
