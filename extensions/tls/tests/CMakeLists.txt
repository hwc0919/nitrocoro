add_executable(tls_test tls_test.cc)
target_link_libraries(tls_test PRIVATE nitrocoro-tls)
