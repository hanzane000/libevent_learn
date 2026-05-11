cmake_minimum_required(VERSION 3.10)
project(libevent_echo_server)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(libevent_echo_server
    src/main.cpp
    src/server.cpp
    src/logger.cpp
)

target_include_directories(libevent_echo_server PRIVATE 
    ${PROJECT_SOURCE_DIR}/include
)

target_link_libraries(libevent_echo_server event)
