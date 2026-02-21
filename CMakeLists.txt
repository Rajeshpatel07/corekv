cmake_minimum_required(VERSION 3.31)
project(corekv VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

add_library(corekv_core
    src/core/db.cpp
    src/core/hash_table.cpp
    src/protocol/serializer.cpp
    src/protocol/parser.cpp
    src/handler/executor.cpp
    src/net/socket.cpp
    src/net/server.cpp
)

target_include_directories(corekv_core PUBLIC ${CMAKE_SOURCE_DIR}/src)

add_executable(corekv-server src/server.cpp)
target_link_libraries(corekv-server PRIVATE corekv_core)

add_executable(corekv-client src/client.cpp)

if(CMAKE_BUILD_TYPE STREQUAL "Release")
    target_compile_options(corekv_core PRIVATE -O3 -march=native -flto)
    target_link_options(corekv_core PRIVATE -flto)
else()
    target_compile_options(corekv_core PRIVATE -O0 -g -Wall -Wextra -Wpedantic)
endif()
