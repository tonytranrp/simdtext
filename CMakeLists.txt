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
option(SIMDTEXT_SANITIZER "Enable sanitizer (address or undefined)" "")
option(SIMDTEXT_COVERAGE "Enable code coverage with gcov/lcov" OFF)

# When included via FetchContent/add_subdirectory, skip extras by default
if(NOT CMAKE_CURRENT_SOURCE_DIR STREQUAL CMAKE_SOURCE_DIR)
    set(SIMDTEXT_BUILD_CLI OFF CACHE BOOL "Build CLI (disabled: subproject)" FORCE)
    set(SIMDTEXT_BUILD_TESTS OFF CACHE BOOL "Build tests (disabled: subproject)" FORCE)
    set(SIMDTEXT_BUILD_EXAMPLES OFF CACHE BOOL "Build examples (disabled: subproject)" FORCE)
    set(SIMDTEXT_BENCHMARKS OFF CACHE BOOL "Build benchmarks (disabled: subproject)" FORCE)
else()
    option(SIMDTEXT_BUILD_TESTS "Build tests" ON)
    option(SIMDTEXT_BUILD_EXAMPLES "Build examples" ON)
endif()

# ── Sanitizer ───────────────────────────────────────────────

if(SIMDTEXT_SANITIZER)
    if(NOT CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        message(WARNING "Sanitizers are only supported on GCC and Clang")
        set(SIMDTEXT_SANITIZER "")
    else()
        set(_san_flags "")
        if(SIMDTEXT_SANITIZER STREQUAL "address")
            set(_san_flags -fsanitize=address -fno-omit-frame-pointer)
        elseif(SIMDTEXT_SANITIZER STREQUAL "undefined")
            set(_san_flags -fsanitize=undefined -fno-omit-frame-pointer -fno-sanitize-recover=all)
        elseif(SIMDTEXT_SANITIZER STREQUAL "address,undefined" OR SIMDTEXT_SANITIZER STREQUAL "undefined,address")
            set(_san_flags -fsanitize=address,undefined -fno-omit-frame-pointer -fno-sanitize-recover=all)
        else()
            message(WARNING "Unknown sanitizer: ${SIMDTEXT_SANITIZER}. Expected: address, undefined, or address,undefined")
            set(SIMDTEXT_SANITIZER "")
        endif()
        if(_san_flags)
            add_compile_options(${_san_flags})
            add_link_options(${_san_flags})
            message(STATUS "Sanitizer enabled: ${SIMDTEXT_SANITIZER}")
        endif()
    endif()
endif()

# ── Coverage ────────────────────────────────────────────────

if(SIMDTEXT_COVERAGE)
    if(NOT CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        message(WARNING "Coverage is only supported on GCC and Clang")
        set(SIMDTEXT_COVERAGE OFF)
    else()
        add_compile_options(--coverage -g -O0)
        add_link_options(--coverage)
        message(STATUS "Coverage enabled")
    endif()
endif()

# ── Highway (SIMD) ─────────────────────────────────────────

if(SIMDTEXT_USE_HIGHWAY)
    # Try find_package first, then fall back to FetchContent
    find_package(hwy QUIET CONFIG)
    if(hwy_FOUND)
        message(STATUS "Highway found via find_package: ${hwy_DIR}")
        add_compile_definitions(SIMDTEXT_HAVE_HWY)
    else()
        # Try bundled subtree
        set(HWY_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/deps/highway")
        if(EXISTS "${HWY_ROOT}/CMakeLists.txt")
            set(HWY_ENABLE_TESTS OFF CACHE BOOL "" FORCE)
            set(HWY_ENABLE_EXAMPLES OFF CACHE BOOL "" FORCE)
            set(HWY_ENABLE_CONTRIB OFF CACHE BOOL "" FORCE)
            add_subdirectory(${HWY_ROOT} deps/highway-build EXCLUDE_FROM_ALL)
            add_compile_definitions(SIMDTEXT_HAVE_HWY)
        else()
            # Fetch Highway via FetchContent as last resort
            include(FetchContent)
            FetchContent_Declare(
                highway
                GIT_REPOSITORY https://github.com/google/highway.git
                GIT_TAG 1.2.0
            )
            set(HWY_ENABLE_TESTS OFF CACHE BOOL "" FORCE)
            set(HWY_ENABLE_EXAMPLES OFF CACHE BOOL "" FORCE)
            set(HWY_ENABLE_CONTRIB OFF CACHE BOOL "" FORCE)
            FetchContent_MakeAvailable(highway)
            if(TARGET hwy)
                add_compile_definitions(SIMDTEXT_HAVE_HWY)
                message(STATUS "Highway fetched via FetchContent")
            else()
                message(WARNING "Highway not available. Building without SIMD Highway backend.")
                set(SIMDTEXT_USE_HIGHWAY OFF)
            endif()
        endif()
    endif()
endif()

# ── Library ─────────────────────────────────────────────────

# ── Intrinsics source files (always compiled, but with per-file flags) ──
set(SIMDTEXT_INTRINSIC_SOURCES
    src/detail/cpu_detect.cpp
    src/detail/simd_dispatch.cpp
    src/detail/simd_scalar.cpp
)

# ── Detect architecture ────────────────────────────────────
if(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|amd64|AMD64|i686|i386|x86")
    set(SIMDTEXT_ARCH_X86 ON)
    set(SIMDTEXT_ARCH_ARM OFF)
elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64|arm64|ARM64|arm")
    set(SIMDTEXT_ARCH_X86 OFF)
    set(SIMDTEXT_ARCH_ARM ON)
else()
    set(SIMDTEXT_ARCH_X86 OFF)
    set(SIMDTEXT_ARCH_ARM OFF)
endif()

set(SIMDTEXT_SOURCES
    src/scalar/scalar.cpp
    src/encode/encode.cpp
    src/url/url.cpp
    src/file/file.cpp
    src/pattern/pattern.cpp
    ${SIMDTEXT_INTRINSIC_SOURCES}
)

if(SIMDTEXT_USE_HIGHWAY)
    list(APPEND SIMDTEXT_SOURCES src/highway/simd_hwy.cpp)
endif()

add_library(simdtext ${SIMDTEXT_SOURCES})

# ── SSE2 intrinsics object (compiled with -msse2) — x86 only ──
if(SIMDTEXT_ARCH_X86)
    add_library(simdtext_sse2 OBJECT src/detail/simd_sse2.cpp)
    target_include_directories(simdtext_sse2 PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(simdtext_sse2 PRIVATE -msse2 -mno-avx)
    elseif(MSVC)
        target_compile_options(simdtext_sse2 PRIVATE /arch:SSE2)
    endif()
    target_link_libraries(simdtext PRIVATE simdtext_sse2)

    # ── AVX2 intrinsics object (compiled with -mavx2) ─────────
    include(CheckCXXCompilerFlag)
    check_cxx_compiler_flag("-mavx2 -mbmi -mlzcnt" COMPILER_SUPPORTS_AVX2)
    if(COMPILER_SUPPORTS_AVX2)
        add_library(simdtext_avx2 OBJECT src/detail/simd_avx2.cpp)
        target_include_directories(simdtext_avx2 PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
        if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
            target_compile_options(simdtext_avx2 PRIVATE -mavx2 -mbmi -mlzcnt -mpopcnt)
        elseif(MSVC)
            target_compile_options(simdtext_avx2 PRIVATE /arch:AVX2)
        endif()
        target_link_libraries(simdtext PRIVATE simdtext_avx2)
    endif()

    # ── AVX-512BW intrinsics object (compiled with -mavx512bw) ──
    check_cxx_compiler_flag("-mavx512bw -mavx512f" COMPILER_SUPPORTS_AVX512BW)
    if(COMPILER_SUPPORTS_AVX512BW)
        add_library(simdtext_avx512 OBJECT src/detail/simd_avx512.cpp)
        target_include_directories(simdtext_avx512 PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
        if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
            target_compile_options(simdtext_avx512 PRIVATE -mavx512bw -mavx512f -mavx512vl -mbmi -mlzcnt -mpopcnt)
        elseif(MSVC)
            target_compile_options(simdtext_avx512 PRIVATE /arch:AVX512)
        endif()
        target_link_libraries(simdtext PRIVATE simdtext_avx512)
    endif()
endif()

# ── NEON intrinsics (ARM) ────────────────────────────────
if(SIMDTEXT_ARCH_ARM)
    add_library(simdtext_neon OBJECT src/detail/simd_neon.cpp)
    target_include_directories(simdtext_neon PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
    target_link_libraries(simdtext PRIVATE simdtext_neon)
endif()

target_include_directories(simdtext PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

# Highway linkage — PUBLIC so consumers get it transitively
if(SIMDTEXT_USE_HIGHWAY)
    if(hwy_FOUND)
        target_link_libraries(simdtext PUBLIC hwy::hwy)
    else()
        target_link_libraries(simdtext PUBLIC hwy)
        target_include_directories(simdtext PRIVATE ${HWY_ROOT} ${CMAKE_CURRENT_SOURCE_DIR})
    endif()
endif()

# Compiler warnings
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(simdtext PRIVATE -Wall -Wextra -Wpedantic -Wconversion)
endif()

# ── C API ──────────────────────────────────────────────────

add_library(simdtext_c src/c/c_api.cpp)
target_include_directories(simdtext_c PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)
target_link_libraries(simdtext_c PUBLIC simdtext)
set_target_properties(simdtext_c PROPERTIES CXX_STANDARD 23 CXX_STANDARD_REQUIRED ON)

# ── Examples ────────────────────────────────────────────────

if(SIMDTEXT_BUILD_EXAMPLES)
    add_executable(example_stats examples/stats.cpp)
    target_link_libraries(example_stats PRIVATE simdtext)

    # Real-world examples
    add_executable(example_log_parser examples/real_world/log_parser.cpp)
    target_link_libraries(example_log_parser PRIVATE simdtext)

    add_executable(example_csv_scanner examples/real_world/csv_scanner.cpp)
    target_link_libraries(example_csv_scanner PRIVATE simdtext)

    add_executable(example_url_handler examples/real_world/url_handler.cpp)
    target_link_libraries(example_url_handler PRIVATE simdtext)

    add_executable(example_utf8_validator examples/real_world/utf8_validator.cpp)
    target_link_libraries(example_utf8_validator PRIVATE simdtext)
endif()

# ── Tests ───────────────────────────────────────────────────

if(SIMDTEXT_BUILD_TESTS)
    add_executable(test_simdtext
        tests/main.cpp
        tests/test_scan.cpp
        tests/test_ascii.cpp
        tests/test_lines.cpp
        tests/test_encode.cpp
        tests/test_url.cpp
        tests/test_utf8.cpp
        tests/test_file.cpp
        tests/test_pattern.cpp
    )
    target_include_directories(test_simdtext PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/tests)
    target_link_libraries(test_simdtext PRIVATE simdtext)
    enable_testing()
    add_test(NAME simdtext_tests COMMAND test_simdtext)
endif()

# ── CLI tool ────────────────────────────────────────────────

if(SIMDTEXT_BUILD_CLI)
    add_executable(simdtext-cli cli/main.cpp)
    target_link_libraries(simdtext-cli PRIVATE simdtext)
    if(SIMDTEXT_USE_HIGHWAY)
        if(hwy_FOUND)
            target_link_libraries(simdtext-cli PRIVATE hwy::hwy)
        else()
            target_link_libraries(simdtext-cli PRIVATE hwy)
        endif()
    endif()
    set_target_properties(simdtext-cli PROPERTIES OUTPUT_NAME simdtext)
endif()

# ── Benchmark (optional) ───────────────────────────────────

if(SIMDTEXT_BENCHMARKS)
    find_package(benchmark QUIET)
    if(NOT benchmark_FOUND)
        message(STATUS "Google Benchmark not found — benchmarks disabled. Install with: pip install benchmark || conan install benchmark")
        set(SIMDTEXT_BENCHMARKS OFF)
    else()
        # Optional: simdutf for UTF-8 validation comparison
        find_package(simdutf QUIET CONFIG)
        if(simdutf_FOUND)
            message(STATUS "simdutf found — UTF-8 validation comparison enabled")
        else()
            message(STATUS "simdutf not found — UTF-8 validation comparison disabled")
        endif()

        add_executable(bench_core benchmarks/bench_core.cpp)
        target_link_libraries(bench_core PRIVATE simdtext benchmark::benchmark)
        if(SIMDTEXT_USE_HIGHWAY)
            if(hwy_FOUND)
                target_link_libraries(bench_core PRIVATE hwy::hwy)
            else()
                target_link_libraries(bench_core PRIVATE hwy)
            endif()
        endif()
        if(simdutf_FOUND)
            target_compile_definitions(bench_core PRIVATE SIMDTEXT_HAVE_SIMDUTF)
            target_link_libraries(bench_core PRIVATE simdutf::simdutf)
        endif()
    endif()
endif()

# ── Install ─────────────────────────────────────────────────

include(GNUInstallDirs)

install(TARGETS simdtext simdtext_c
    EXPORT simdtext-targets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

if(SIMDTEXT_ARCH_X86)
    install(TARGETS simdtext_sse2
        EXPORT simdtext-targets
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    )
    if(COMPILER_SUPPORTS_AVX2)
        install(TARGETS simdtext_avx2
            EXPORT simdtext-targets
            ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        )
    endif()
    if(COMPILER_SUPPORTS_AVX512BW)
        install(TARGETS simdtext_avx512
            EXPORT simdtext-targets
            ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        )
    endif()
endif()

if(SIMDTEXT_ARCH_ARM)
    install(TARGETS simdtext_neon
        EXPORT simdtext-targets
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    )
endif()

install(DIRECTORY include/simdtext
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

if(SIMDTEXT_BUILD_CLI)
    install(TARGETS simdtext-cli RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
endif()

# ── CMake package config ───────────────────────────────────

include(CMakePackageConfigHelpers)

write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/simdtext-config-version.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)

install(EXPORT simdtext-targets
    FILE simdtext-targets.cmake
    NAMESPACE simdtext::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/simdtext
)

configure_package_config_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/simdtext-config.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/simdtext-config.cmake"
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/simdtext
)

install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/simdtext-config.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/simdtext-config-version.cmake"
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/simdtext
)
