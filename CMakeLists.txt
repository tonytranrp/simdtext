cmake_minimum_required(VERSION 3.16)
project(simdtext VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ── Library ─────────────────────────────────────────────────

add_library(simdtext
    src/scalar/scalar.cpp
    src/encode/encode.cpp
    src/url/url.cpp
    src/file/file.cpp
)

target_include_directories(simdtext PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

# Compiler warnings
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(simdtext PRIVATE -Wall -Wextra -Wpedantic -Wconversion)
endif()

# ── Examples ────────────────────────────────────────────────

add_executable(example_stats examples/stats.cpp)
target_link_libraries(example_stats PRIVATE simdtext)

add_executable(test_simdtext examples/test.cpp)
target_link_libraries(test_simdtext PRIVATE simdtext)

# ── Benchmark (optional) ───────────────────────────────────

option(SIMDTEXT_BENCHMARKS "Build benchmarks" OFF)

if(SIMDTEXT_BENCHMARKS)
    find_package(benchmark REQUIRED)
    add_executable(bench_count_newlines benchmarks/bench_count_newlines.cpp)
    target_link_libraries(bench_count_newlines PRIVATE simdtext benchmark::benchmark)
endif()

# ── Install ─────────────────────────────────────────────────

include(GNUInstallDirs)

install(TARGETS simdtext
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

install(DIRECTORY include/simdtext
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)
