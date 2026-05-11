cmake_minimum_required(VERSION 3.10)
project(evbuffer)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(evbuffer src/evbuffer_basic_demo.cpp)
target_link_libraries(evbuffer event)