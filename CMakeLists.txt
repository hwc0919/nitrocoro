cmake_minimum_required(VERSION 3.15)
project(nitro_coro CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_FLAGS "-ggdb")

# Build library
add_library(nitro_coro STATIC
    src/Scheduler.cc
    src/TcpServer.cc
    src/TcpConnection.cc
    src/IoChannel.cc
)

target_include_directories(nitro_coro PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# Build tests
add_subdirectory(tests)
