#ifndef DAWNSEEKER_GLOBAL_H
#define DAWNSEEKER_GLOBAL_H

#include <chrono>
#include <filesystem>
#include <numbers>
#include <ranges>
#include <version>

// Abbreviations for std sub-namespaces
namespace ch = std::chrono;
namespace fs = std::filesystem;
namespace ns = std::numbers; 
namespace rs = std::ranges;
namespace vs = std::views; 

// Enables all the literals in namespace std
using namespace std::literals;

#if __cpp_lib_format 
#include <format> 
using std::format; 
#define FORMAT_NAMESPACE std
#define FORMAT(...) std::format(__VA_ARGS__)
#elif __has_include(<fmt/format.h>)
#include <fmt/chrono.h>
#include <fmt/format.h>
using fmt::format; 
#define FORMAT_NAMESPACE fmt
#define FORMAT(...) fmt::format(__VA_ARGS__)
#else 
#error "Either std::format in C++20 or fmt::format in libfmt should be supported!"
#endif

#endif //DAWNSEEKER_GLOBAL_H