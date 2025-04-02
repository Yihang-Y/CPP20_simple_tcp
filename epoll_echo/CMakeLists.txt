cmake_minimum_required(VERSION 3.5)
project(epoll_echo)

# set cmake build type
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Debug")
endif()

set(CMAKE_C_COMPILER "clang")
set(CMAKE_CXX_COMPILER "clang++")

if (CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wno-unknown-pragmas -std=c++20 ${CXX_LIB_OPTN} -fsanitize=address -g -O0")
else()
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wno-unknown-pragmas -std=c++20 ${CXX_LIB_OPTN} -O3")
endif()
include_directories(../common ./)
add_executable(epoll_echo 
    main.cpp
    EPoller.cpp
    EventLoop.cpp
    Channel.cpp
)