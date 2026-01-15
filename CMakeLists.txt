cmake_minimum_required(VERSION 3.31)
project("corekv")

set(CXX_MINIMUM_STANDARD 17)
set(CXX_STANDARD_REQUIRED ON)

add_executable(
corekv-server 
server.cpp
src/hashtable.hpp
src/hashtable.cpp
src/utils/socket.hpp
src/utils/socket.cpp
src/utils/parser.hpp
src/utils/parser.cpp
)
add_executable(
corekv-client
client.cpp
)

if(CMAKE_BUILD_TYPE STREQUAL "Release") 
	# -O0 No optimization
	# -O3 for faster performance
	# -O2 for balanced performance
	# -Os for smaller file size
	# -march=native binaries according to system Architecture
	# -flto  for Link-time optimization

	target_compile_options(corekv-server PRIVATE -O3)
else()
	target_compile_options(corekv-server PRIVATE -O0)	
endif()
