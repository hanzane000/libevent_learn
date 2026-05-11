cmake_minimum_required(VERSION 3.10)
project(echo)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(echo echo.cpp)
target_link_libraries(echo event)