cmake_minimum_required(VERSION 3.10)
project(day1_libevent_hello)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 关键：让CMake找到 libevent 的头文件和库
include_directories(/usr/local/include)
link_directories(/usr/local/lib)

add_executable(day1_libevent_hello main.cpp)
target_link_libraries(day1_libevent_hello event)
