add_executable(coro_test
    coro_test.cc
)

target_link_libraries(coro_test PRIVATE nitro_coro)

add_executable(tcp_echo_test
    tcp_echo_test.cc
)

target_link_libraries(tcp_echo_test PRIVATE nitro_coro)

add_executable(tcp_chat_test
    tcp_chat_test.cc
)

target_link_libraries(tcp_chat_test PRIVATE nitro_coro)
