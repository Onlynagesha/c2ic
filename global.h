#ifndef DAWNSEEKER_GLOBAL_H
#define DAWNSEEKER_GLOBAL_H

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

// Support for syntax like
// with (auto timer = Timer()) { /* do sth. */ }
#define with(sth) if (sth; true)

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

template <class I> requires std::integral<I> || std::floating_point<I>
constexpr I halfMax = std::numeric_limits<I>::max() / 2;

template <class I> requires std::integral<I> || std::floating_point<I>
constexpr I halfMin = std::numeric_limits<I>::lowest() / 2;

inline auto createMT19937Generator(unsigned initialSeed = 0) noexcept {
    return std::mt19937(initialSeed != 0 ? initialSeed : std::random_device()());
}

#endif //DAWNSEEKER_GLOBAL_H