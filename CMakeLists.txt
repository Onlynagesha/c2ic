cmake_minimum_required(VERSION 3.21)
project(Graph)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_FLAGS_RELEASE "-O3")

if (UNIX)
link_libraries(fmt)
endif()

include_directories(.)

add_executable(Graph playground.cpp argparse/argparse.hpp
        global.h Logger.h imm.cpp imm.h immbasic.h PRRGraph.cpp PRRGraph.h
        Timer.h args.h args.cpp immbasic.cpp args-v2.h args-v2.cpp)
#add_executable(Graph playground.cpp)