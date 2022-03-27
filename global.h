#ifndef DAWNSEEKER_GLOBAL_H
#define DAWNSEEKER_GLOBAL_H

#include "utils.h"
#include <chrono>
#include <filesystem>
#include <limits>
#include <numbers>
#include <random>
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

// +inf
template <class I> requires std::integral<I> || std::floating_point<I>
constexpr I halfMax = std::numeric_limits<I>::max() / 2;

// -inf
template <class I> requires std::integral<I> || std::floating_point<I>
constexpr I halfMin = std::numeric_limits<I>::lowest() / 2;
#endif //DAWNSEEKER_GLOBAL_H