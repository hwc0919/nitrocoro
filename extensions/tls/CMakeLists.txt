set(NITROCORO_TLS_PROVIDER "openssl" CACHE STRING "TLS provider: openssl / botan")

add_library(nitrocoro-tls
    src/TlsStream.cc
)

target_include_directories(nitrocoro-tls
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_link_libraries(nitrocoro-tls
    PUBLIC
        nitrocoro
)

if(NITROCORO_TLS_PROVIDER STREQUAL "openssl")
    find_package(OpenSSL REQUIRED)
    target_sources(nitrocoro-tls PRIVATE src/providers/OpenSSLProvider.cc)
    target_link_libraries(nitrocoro-tls PRIVATE OpenSSL::SSL OpenSSL::Crypto)
elseif(NITROCORO_TLS_PROVIDER STREQUAL "botan")
    find_package(Botan REQUIRED)
    target_sources(nitrocoro-tls PRIVATE src/providers/BotanProvider.cc)
    target_link_libraries(nitrocoro-tls PRIVATE Botan::Botan)
else()
    message(FATAL_ERROR "Unknown NITROCORO_TLS_PROVIDER: ${NITROCORO_TLS_PROVIDER}")
endif()

add_subdirectory(tests)
