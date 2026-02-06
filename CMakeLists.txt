cmake_minimum_required(VERSION 3.5)
project(coro CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_FLAGS "-ggdb")

# Build library
add_library(coro_lib STATIC
    src/CoroScheduler.cc
    src/TcpServer.cc
    src/TcpClient.cc
    src/TcpConnection.cc
    src/IoChannel.cc
)

target_include_directories(coro_lib PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# Build tests
add_subdirectory(tests)
