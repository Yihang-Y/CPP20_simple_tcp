cmake_minimum_required(VERSION 3.2)
project(simple_tcp)

set(CMAKE_C_COMPILER "clang")
set(CMAKE_CXX_COMPILER "clang++")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wno-unknown-pragmas -std=c++20 ${CXX_LIB_OPTN} -fsanitize=address")

add_executable(simple_tcp main.cpp)

target_link_libraries(simple_tcp PRIVATE uring)
