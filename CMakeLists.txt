cmake_minimum_required(VERSION 3.31)
project("corekv")

set(CXX_MINIMUM_STANDARD 17)
set(CXX_STANDARD_REQUIRED ON)

add_executable(
corekv-server 
server.cpp
utils/socket.hpp
utils/socket.cpp
utils/parser.hpp
utils/parser.cpp
)
add_executable(
corekv-client
client.cpp
)
