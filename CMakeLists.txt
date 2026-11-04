cmake_minimum_required(VERSION 3.20)
project(corekv VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE "Debug" CACHE STRING "Choose the build type (Debug, Release)" FORCE)
endif()

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

function(corekv_enable_warnings target)
  if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(${target} PRIVATE
      -Wall
      -Wextra
      -Wpedantic
      -Wconversion
      -Wsign-conversion
      -Wshadow
      -Wnon-virtual-dtor
      -Wold-style-cast
    )
  endif()
endfunction()

add_library(corekv_core STATIC
  src/command/executor.cpp
  src/db/db.cpp
  src/db/hashtable.cpp
  src/net/epoll.cpp
  src/net/socket.cpp
  src/parser/parser.cpp
  src/parser/resp.cpp
  src/server/server.cpp
)

add_library(corekv::core ALIAS corekv_core)

target_include_directories(corekv_core PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/src")
target_compile_features(corekv_core PUBLIC cxx_std_20)
corekv_enable_warnings(corekv_core)

target_compile_options(corekv_core PRIVATE
  $<$<CONFIG:Release>:-O3 -DNDEBUG>
  $<$<CONFIG:Debug>:-g -O0>
)

add_executable(corekv src/main.cpp)
target_link_libraries(corekv PRIVATE corekv::core)
corekv_enable_warnings(corekv)

target_compile_options(corekv PRIVATE
  $<$<CONFIG:Release>:-O3 -DNDEBUG>
  $<$<CONFIG:Debug>:-g -O0>
)
