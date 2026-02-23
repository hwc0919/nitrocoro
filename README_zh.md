# Nitro Coro

中文 | [English](README.md)

基于 C++20 协程的**高性能异步网络框架**（开发中）。

> **注意**：
> - 本项目正在开发中
> - 目前仅支持 Linux，使用 epoll 进行 I/O 多路复用
> - HttpServer/TcpServer 均为基础 demo，功能和生命周期管理都不完整

## 项目目标

打造一个生产级的高性能异步网络框架，提供：
- 高性能的事件驱动架构，性能对标 Drogon 框架
- 简洁易用的协程 API，消除回调地狱
- 跨平台支持（Linux、Windows、macOS）
- 完善的网络协议支持（TCP、HTTP、WebSocket 等）
- 完善的组件生命周期管理
- 生产级的稳定性和可靠性

## 特性

- **原生协程支持**：使用 C++20 协程实现异步 I/O 和任务调度，无需回调函数
- **协程调度器**：基于 epoll 的事件驱动调度器
- **协程原语**：Task、Future、Promise、Mutex
- **基础网络演示**：TCP/HTTP 服务器和客户端作为演示
- **异步 DNS 解析**：协程友好的 DNS 查询

## 系统要求

- 支持 C++20 的编译器（GCC 10+、Clang 12+）
- CMake 3.15+
- Linux（目前仅支持 Linux，跨平台支持计划中）

## 快速开始

### 编译

```bash
mkdir build && cd build
cmake ..
make
```

### 运行示例

```bash
# TCP Echo 服务器
./examples/tcp_echo_server 8888

# TCP 客户端
./examples/tcp_client 8888 127.0.0.1

# HTTP 服务器
./examples/http_server 8080
# 然后访问: curl http://localhost:8080/

# HTTP 客户端
./examples/http_client http://example.com/
```

## 示例代码

使用协程的简单 HTTP 服务器：

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

## 项目结构

```
coro/
├── include/nitro_coro/    # 头文件
│   ├── core/              # 协程原语（Task、Scheduler、Future 等）
│   ├── net/               # 网络组件（TCP、DNS）
│   ├── http/              # HTTP 服务器/客户端
│   ├── io/                # 异步 I/O
│   └── utils/             # 工具类
├── src/                   # 实现文件
├── examples/              # 示例程序
└── tests/                 # 单元测试
```

## 示例程序

- `tcp_echo_server`：TCP echo 服务器演示
- `tcp_client`：带异步 DNS 解析的 TCP 客户端
- `tcp_chat_server`：简单聊天服务器
- `http_server`：带路由的 HTTP 服务器
- `http_client`：HTTP 客户端演示

## TODO

- [ ] 完善功能
- [ ] 跨平台支持（Windows、macOS）
- [ ] 更多协程运行时特性
- [ ] 性能优化
- [ ] API 文档
- [ ] 更全面的示例

## 许可证

MIT License
