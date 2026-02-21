# Nitro Coro

[中文](README_zh.md) | English

A **high-performance coroutine-based async network framework** (under development) built on C++20 coroutines.

> **Note**: 
> - This project is under development
> - Currently Linux-only, using epoll for I/O multiplexing
> - HttpServer/TcpServer are basic demos with incomplete features and lifecycle management

## Project Goals

Build a production-grade high-performance async network framework that provides:
- High-performance event-driven architecture, targeting Drogon framework performance
- Clean and easy-to-use coroutine API, eliminating callback hell
- Cross-platform support (Linux, Windows, macOS)
- Comprehensive network protocol support (TCP, HTTP, WebSocket, etc.)
- Comprehensive component lifecycle management
- Production-grade stability and reliability

## Features

- **Native coroutine support**: Async I/O and task scheduling implemented with C++20 coroutines, no callbacks needed
- **Coroutine Scheduler**: Event-driven scheduler with epoll backend
- **Coroutine Primitives**: Task, Future, Promise, Mutex
- **Basic Network Demos**: TCP/HTTP server and client as demonstrations
- **Async DNS Resolution**: Coroutine-friendly DNS queries

## Requirements

- C++20 compatible compiler (GCC 10+, Clang 12+)
- CMake 3.15+
- Linux (currently Linux-only, cross-platform support planned)

## Quick Start

### Build

```bash
mkdir build && cd build
cmake ..
make
```

### Run Examples

```bash
# TCP Echo Server
./tests/tcp_echo_server 8888

# TCP Client
./tests/tcp_client 8888 127.0.0.1

# HTTP Server
./tests/http_server 8080
# Then visit: curl http://localhost:8080/

# HTTP Client
./tests/http_client http://example.com/
```

## Example Code

Simple HTTP server using coroutines:

```cpp
Task<> server_main(uint16_t port)
{
    HttpServer server(port);
    
    server.route("GET", "/", [](HttpRequest& req, HttpResponse& resp) -> Task<> {
        resp.setStatus(200);
        resp.setHeader("Content-Type", "text/html");
        co_await resp.end("<h1>Hello, World!</h1>");
    });
    
    co_await server.start();
}

int main() {
    Scheduler scheduler;
    scheduler.spawn([]() -> Task<> { co_await server_main(8080); });
    scheduler.run();
    return 0;
}
```

## Project Structure

```
coro/
├── include/nitro_coro/    # Header files
│   ├── core/              # Coroutine primitives (Task, Scheduler, Future, etc.)
│   ├── net/               # Network components (TCP, DNS)
│   ├── http/              # HTTP server/client
│   ├── io/                # Async I/O
│   └── utils/             # Utilities
├── src/                   # Implementation files
└── tests/                 # Demo programs
```

## Demo Programs

- `tcp_echo_server`: TCP echo server demo
- `tcp_client`: TCP client with async DNS resolution
- `tcp_chat_server`: Simple chat server
- `http_server`: HTTP server with routing
- `http_client`: HTTP client demo
- `dns_resolver_test`: Async DNS resolver test

## TODO

- [ ] Complete features and functionality
- [ ] Cross-platform support (Windows, macOS)
- [ ] More coroutine runtime features
- [ ] Performance optimization
- [ ] API documentation
- [ ] More comprehensive examples

## License

MIT License
