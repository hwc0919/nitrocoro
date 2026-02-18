cmake_minimum_required(VERSION 3.15)
project(nitro_coro CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug)
endif()

set(CMAKE_CXX_FLAGS_DEBUG "-g")
set(CMAKE_CXX_FLAGS_RELEASE "-O2 -DNDEBUG")

# Build library
add_library(nitro_coro STATIC
    src/Scheduler.cc
    src/TcpServer.cc
    src/TcpConnection.cc
    src/IoChannel.cc
    src/HttpRequest.cc
    src/HttpResponse.cc
    src/HttpServer.cc
    src/HttpClient.cc
    src/InetAddress.cc
    src/DnsResolver.cc
)

target_include_directories(nitro_coro PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# Link pthread for threading support
find_package(Threads REQUIRED)
target_link_libraries(nitro_coro PUBLIC Threads::Threads)

# Build tests
add_subdirectory(tests)
