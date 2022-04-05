cmake_minimum_required(VERSION 3.21)
project(Graph)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_FLAGS_RELEASE "-O3")

if (UNIX)
link_libraries(fmt)
endif()

include_directories(.)

#add_executable(Graph main.cpp argparse/argparse.hpp
#        global.h Logger.h imm.cpp imm.h immbasic.h PRRGraph.cpp PRRGraph.h
#        Timer.h args.h args.cpp cstr.h immbasic.cpp args-v2.h)
add_executable(Graph playground.cpp playground.cpp utils/assert.h utils/typeinfo.h utils/type_traits.h utils/numeric.h utils/random.h utils/cstring.h utils/string.h utils/format.h utils/misc.h utils/all.h args/basic.h args/exception.h args/element.h args/variant.h args/any.h args/args.h)
