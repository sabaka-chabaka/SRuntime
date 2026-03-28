cmake_minimum_required(VERSION 3.20)
project(SabakaRuntime CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG -march=native")
set(CMAKE_CXX_FLAGS_DEBUG   "-g -fsanitize=address,undefined")

# ── VM + BinaryReader ─────────────────────────────────────────────────────────
add_library(sabaka_core STATIC
    src/sabaka_types.cpp
    src/virtual_machine.cpp
    src/binary_reader.cpp
)
target_include_directories(sabaka_core PUBLIC include)

# ── HTTP (опционально, libcurl) ───────────────────────────────────────────────
option(SABAKA_HTTP "Enable HTTP via libcurl" OFF)
if(SABAKA_HTTP)
    find_package(CURL REQUIRED)
    target_link_libraries(sabaka_core PUBLIC CURL::libcurl)
    target_compile_definitions(sabaka_core PUBLIC SABAKA_HTTP)
endif()

# ── Лаунчер ───────────────────────────────────────────────────────────────────
add_executable(sre src/main.cpp)
target_link_libraries(sre PRIVATE sabaka_core)

# ── Тесты ─────────────────────────────────────────────────────────────────────
enable_testing()
add_executable(sabaka_tests src/tests.cpp)
target_link_libraries(sabaka_tests PRIVATE sabaka_core)
add_test(NAME SabakaTests COMMAND sabaka_tests)
