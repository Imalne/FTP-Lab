cmake_minimum_required(VERSION 3.12)
project(server C)
set(CMAKE_C_STANDARD 99)

add_executable(server main.c handler.c server.h)
find_package(Threads REQUIRED)
target_link_libraries(server Threads::Threads)
