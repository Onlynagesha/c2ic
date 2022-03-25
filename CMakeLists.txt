cmake_minimum_required(VERSION 3.21)
project(Graph)

set(CMAKE_CXX_STANDARD 23)

include_directories(argparse)
add_executable(Graph main.cpp argparse/argparse.hpp
        global.h Logger.h imm.cpp imm.h immbasic.h PRRGraph.cpp PRRGraph.h Timer.h args.h args.cpp)
