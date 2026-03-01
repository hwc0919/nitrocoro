enable_testing()

add_executable(tls_test tls_test.cc)
target_link_libraries(tls_test PRIVATE nitrocoro-tls)
add_test(NAME tls_test COMMAND tls_test)
