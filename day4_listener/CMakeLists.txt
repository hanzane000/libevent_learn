cmake_minimum_required(VERSION 3.10)
project(listener)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(accept_listener accept_listener_demo.cpp)
target_link_libraries(accept_listener event)