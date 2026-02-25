add_executable(coro_test coro_test.cc)
target_link_libraries(coro_test PRIVATE nitrocoro)

add_executable(generator_test generator_test.cc)
target_link_libraries(generator_test PRIVATE nitrocoro)

add_executable(http_stream_test http_stream_test.cc)
target_link_libraries(http_stream_test PRIVATE nitrocoro)

add_executable(dns_resolver_test dns_resolver_test.cc)
target_link_libraries(dns_resolver_test PRIVATE nitrocoro)
