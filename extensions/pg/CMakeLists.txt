find_package(PostgreSQL REQUIRED)

add_library(nitrocoro-pg STATIC
    src/PgConnection.cc
)

target_include_directories(nitrocoro-pg PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(nitrocoro-pg PUBLIC
    nitrocoro
    PostgreSQL::PostgreSQL
)

add_executable(pg_test tests/pg_test.cc)
target_link_libraries(pg_test PRIVATE nitrocoro-pg)
