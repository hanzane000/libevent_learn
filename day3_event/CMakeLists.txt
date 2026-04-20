cmake_minimum_required(VERSION 3.10)
project(evnet_and_listener)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(stdin_read_event stdin_read_event.cpp)
target_link_libraries(stdin_read_event event)
