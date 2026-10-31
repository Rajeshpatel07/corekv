cmake_minimum_required(VERSION 3.20)
project(corekv VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Keep warnings in one place so every target is compiled with the same rules.
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

# The db sources are compiled even though the server does not use them yet, so
# they cannot silently rot. C++20 is requested project-wide and also propagated
# to consumers through target_compile_features.
add_library(corekv_core STATIC
  src/db/hashtable.cpp
  src/net/epoll.cpp
  src/net/socket.cpp
  src/parser/parser.cpp
  src/parser/resp.cpp
  src/server/server.cpp
)

# Namespaced alias: consumers link corekv::core, never a bare target name.
add_library(corekv::core ALIAS corekv_core)

# PUBLIC because anything linking corekv_core needs the project headers too.
target_include_directories(corekv_core PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/src")
target_compile_features(corekv_core PUBLIC cxx_std_20)
corekv_enable_warnings(corekv_core)

add_executable(corekv src/main.cpp)
target_link_libraries(corekv PRIVATE corekv::core)
corekv_enable_warnings(corekv)
