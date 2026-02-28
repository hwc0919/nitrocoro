find_package(OpenSSL REQUIRED)

add_library(nitrocoro-tls
    src/TlsConnection.cc
)

target_include_directories(nitrocoro-tls
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_link_libraries(nitrocoro-tls
    PUBLIC
        nitrocoro
        OpenSSL::SSL
        OpenSSL::Crypto
)

add_subdirectory(tests)
