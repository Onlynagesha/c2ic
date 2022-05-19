cmake_minimum_required(VERSION 3.21)
project(Graph)

set(CMAKE_CXX_STANDARD 23)

# Disables all log messages
add_definitions(-DLOG_LEVEL=4)
include_directories(.)

add_executable(Graph main-v2.cpp imm-v2.cpp PRRGraph.h PRRGraph.cpp args-v2.cpp)
