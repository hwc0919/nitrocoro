cmake_minimum_required(VERSION 3.15)
project(nitrocoro CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug)
endif()

set(CMAKE_CXX_FLAGS_DEBUG "-g")
set(CMAKE_CXX_FLAGS_RELEASE "-O2 -DNDEBUG")

# Build library
add_library(nitrocoro STATIC
    src/Scheduler.cc
    src/TcpServer.cc
    src/TcpConnection.cc
    src/IoChannel.cc
    src/InetAddress.cc
    src/DnsResolver.cc
    src/Dns.cc
    src/Url.cpp
)

target_include_directories(nitrocoro PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# Link pthread for threading support
find_package(Threads REQUIRED)
target_link_libraries(nitrocoro PUBLIC Threads::Threads)

# Build tests
add_subdirectory(tests)

# Build examples
add_subdirectory(examples)

# Extensions
option(BUILD_EXT_HTTP "Build HTTP extension" ON)
if(BUILD_EXT_HTTP)
    add_subdirectory(extensions/http)
endif()

option(BUILD_EXT_PG "Build PostgreSQL extension" OFF)
if(BUILD_EXT_PG)
    add_subdirectory(extensions/pg)
endif()

option(BUILD_EXT_TLS "Build TLS extension" OFF)
if(BUILD_EXT_TLS)
    add_subdirectory(extensions/tls)
endif()