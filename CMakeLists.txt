cmake_minimum_required(VERSION 3.21)
project(Graph)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_FLAGS_RELEASE "-O3")

if (UNIX)
link_libraries(fmt pthread)
endif()

include_directories(.)

add_executable(Graph main-v2.cpp imm-v2.cpp PRRGraph.h PRRGraph.cpp args-v2.cpp ProgressCounter.h thread.h graphbasic.h)
#add_executable(Graph playground.cpp)
