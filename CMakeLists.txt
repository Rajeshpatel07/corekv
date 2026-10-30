cmake_minimum_required(VERSION 3.31)
project("corekv" LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_library(corekv_header INTERFACE)
target_include_directories(corekv_header INTERFACE src)

add_library(corekv_source STATIC src/core/network.cpp src/parser/parser.cpp)
target_link_libraries(corekv_source PRIVATE corekv_header)

add_executable(corekv main.cpp)

target_link_libraries(corekv PRIVATE corekv_header corekv_source)
