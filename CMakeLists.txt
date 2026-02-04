cmake_minimum_required(VERSION 3.5)
project(coro CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_FLAGS "-ggdb")

add_executable(coro
    main.cc
    CoroScheduler.cc
)

target_include_directories(coro PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

add_executable(tcp_test
    tcp_test.cc
    CoroScheduler.cc
    TcpServer.cc
)

target_include_directories(tcp_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
