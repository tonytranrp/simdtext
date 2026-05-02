cmake_minimum_required(VERSION 3.16)
project(simdtext VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ── Options ─────────────────────────────────────────────────

option(SIMDTEXT_BENCHMARKS "Build benchmarks" OFF)
option(SIMDTEXT_USE_HIGHWAY "Enable SIMD via Google Highway" ON)
option(SIMDTEXT_BUILD_CLI "Build the simdtext CLI tool" ON)

# ── Highway (SIMD) ─────────────────────────────────────────

if(SIMDTEXT_USE_HIGHWAY)
    set(HWY_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/deps/highway")
    if(EXISTS "${HWY_ROOT}/CMakeLists.txt")
        set(HWY_ENABLE_TESTS OFF CACHE BOOL "" FORCE)
        set(HWY_ENABLE_EXAMPLES OFF CACHE BOOL "" FORCE)
        set(HWY_ENABLE_CONTRIB OFF CACHE BOOL "" FORCE)
        add_subdirectory(${HWY_ROOT} deps/highway-build EXCLUDE_FROM_ALL)
        add_compile_definitions(SIMDTEXT_HAVE_HWY)
    else()
        message(WARNING "Highway not found at ${HWY_ROOT}. Building without SIMD.")
        set(SIMDTEXT_USE_HIGHWAY OFF)
    endif()
endif()

# ── Library ─────────────────────────────────────────────────

set(SIMDTEXT_SOURCES
    src/scalar/scalar.cpp
    src/encode/encode.cpp
    src/url/url.cpp
    src/file/file.cpp
)

if(SIMDTEXT_USE_HIGHWAY)
    list(APPEND SIMDTEXT_SOURCES src/highway/simd_hwy.cpp)
endif()

add_library(simdtext ${SIMDTEXT_SOURCES})

target_include_directories(simdtext PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

if(SIMDTEXT_USE_HIGHWAY)
    target_link_libraries(simdtext PRIVATE hwy)
    target_include_directories(simdtext PRIVATE ${HWY_ROOT} ${CMAKE_CURRENT_SOURCE_DIR})
endif()

# Compiler warnings
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(simdtext PRIVATE -Wall -Wextra -Wpedantic -Wconversion)
endif()

# ── Examples ────────────────────────────────────────────────

add_executable(example_stats examples/stats.cpp)
target_link_libraries(example_stats PRIVATE simdtext)

add_executable(test_simdtext
    tests/main.cpp
    tests/test_scan.cpp
    tests/test_ascii.cpp
    tests/test_lines.cpp
    tests/test_encode.cpp
    tests/test_url.cpp
    tests/test_utf8.cpp
    tests/test_file.cpp
)
target_include_directories(test_simdtext PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/tests)
target_link_libraries(test_simdtext PRIVATE simdtext)

# ── CLI tool ────────────────────────────────────────────────

if(SIMDTEXT_BUILD_CLI)
    add_executable(simdtext-cli cli/main.cpp)
    target_link_libraries(simdtext-cli PRIVATE simdtext)
    if(SIMDTEXT_USE_HIGHWAY)
        target_link_libraries(simdtext-cli PRIVATE hwy)
    endif()
    set_target_properties(simdtext-cli PROPERTIES OUTPUT_NAME simdtext)
endif()

# ── Benchmark (optional) ───────────────────────────────────

if(SIMDTEXT_BENCHMARKS)
    find_package(benchmark REQUIRED)
    add_executable(bench_core benchmarks/bench_core.cpp)
    target_link_libraries(bench_core PRIVATE simdtext benchmark::benchmark)
    if(SIMDTEXT_USE_HIGHWAY)
        target_link_libraries(bench_core PRIVATE hwy)
    endif()
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

if(SIMDTEXT_BUILD_CLI)
    install(TARGETS simdtext-cli RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
endif()
