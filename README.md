# NitroCoro

[中文](README_zh.md) | English

A **coroutine async I/O runtime** built on C++20 coroutines, with a high-performance, elegantly designed core and a full-stack ecosystem via official extensions.

> [!NOTE]
> Under active development.
> - Cross-platform planned (currently Linux only, epoll)
> - Core API is stabilizing
> - Lifecycle management is not yet complete
> - Extension API is still being explored, no stability guarantee

## Design Goals

The ultimate goal is a high-performance network application framework — from bare async I/O all the way up to a full web framework. NitroCoro is the core runtime that everything builds on, designed around three principles:

- **High-performance core** — minimal design, zero-overhead abstractions, `cmake .. && make`
- **Out-of-the-box HTTP** — HTTP/1.1 server and client included by default
- **Extensible ecosystem** — TLS, HTTP/2, WebSocket, databases via official extensions

## Features

- **Native coroutine support** — async I/O and task scheduling via C++20 coroutines, no callbacks
- **Coroutine Scheduler** — epoll-based event loop with timer support and cross-thread wakeup
- **Coroutine primitives** — Task, Future, Promise, Mutex, Generator
- **TCP networking** — TcpServer, TcpConnection, async DNS resolution
- **HTTP/1.1** — HTTP server with routing and client, streaming request/response body

## Architecture

```
web application framework (planned)
    ↑
nitrocoro (this repo)
├── core        Scheduler / Task / Future / Mutex / Generator
├── io          IoChannel / Stream interface
├── net         TcpServer / TcpConnection / DNS
└── extensions/
    ├── http        HTTP/1.1 server + client          [default ON]
    ├── tls         TLS via OpenSSL                   [default OFF]
    ├── http2       HTTP/2                            [default OFF]
    └── websocket   WebSocket                         [default OFF]

github.com/nitrocoro/ (planned, version-independent)
├── nitrocoro-pg
├── nitrocoro-redis
└── nitrocoro-mysql
```

## Requirements

- C++20 compiler (GCC 10+, Clang 12+)
- CMake 3.15+
- Linux (epoll) — cross-platform (Windows, macOS) planned

## Quick Start

```bash
git clone https://github.com/nitrocoro/nitrocoro
cd nitrocoro && mkdir build && cd build
cmake .. && make
```

```bash
./examples/tcp_echo_server 8888
./examples/http_server 8080   # curl http://localhost:8080/
./examples/http_client http://example.com/
```

## Example

```cpp
#include <nitrocoro/http/HttpServer.h>
using namespace nitrocoro;

Task<> run()
{
    HttpServer server(8080);
    server.route("GET", "/", [](HttpRequest & req, HttpResponse & resp) -> Task<> {
        co_await resp.end("<h1>Hello, NitroCoro!</h1>");
    });
    co_await server.start();
}

int main()
{
    Scheduler scheduler;
    scheduler.spawn([]() -> Task<> { co_await run(); });
    scheduler.run();
}
```

## Project Structure

```
nitrocoro/
├── include/nitrocoro/
│   ├── core/           Task, Scheduler, Future, Mutex, Generator
│   ├── net/            TcpServer, TcpConnection, DNS
│   ├── io/             IoChannel, Stream, adapters
│   └── utils/          Debug macros, buffers
├── src/                Core implementation
├── extensions/
│   └── http/           HTTP/1.1 extension
├── examples/
└── tests/
```

## Roadmap

- [ ] TLS extension (OpenSSL)
- [ ] HTTP/2 extension
- [ ] WebSocket extension
- [ ] `install()` + `find_package()` support
- [ ] Cross-platform (Windows, macOS)
- [ ] Upper-layer web application framework

## License

MIT License
