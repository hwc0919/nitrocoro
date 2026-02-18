add_executable(coro_test
    coro_test.cc
)

target_link_libraries(coro_test PRIVATE nitro_coro)

add_executable(tcp_echo_server
    tcp_echo_server.cc
)

target_link_libraries(tcp_echo_server PRIVATE nitro_coro)

add_executable(tcp_chat_server
    tcp_chat_server.cc
)

target_link_libraries(tcp_chat_server PRIVATE nitro_coro)

add_executable(tcp_client
    tcp_client.cc
)

target_link_libraries(tcp_client PRIVATE nitro_coro)

add_executable(http_server
    http_server.cc
)

target_link_libraries(http_server PRIVATE nitro_coro)

add_executable(http_client
    http_client.cc
)

target_link_libraries(http_client PRIVATE nitro_coro)

add_executable(dns_resolver_test
    dns_resolver_test.cc
)

target_link_libraries(dns_resolver_test PRIVATE nitro_coro)
