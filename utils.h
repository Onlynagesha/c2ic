//
// Created by Onlynagesha on 2022/3/27.
//

#ifndef GRAPH_UTILS_H
#define GRAPH_UTILS_H

#include "global.h"
#include <cctype>
#include <charconv>
#include <chrono>
#include <concepts>
#include <limits>
#include <random>
#include <ranges>
#include <string>
#include <string_view>
#include <type_traits>

// +inf
template <class I> requires std::integral<I> || std::floating_point<I>
constexpr I halfMax = std::numeric_limits<I>::max() / 2;

// -inf
template <class I> requires std::integral<I> || std::floating_point<I>
constexpr I halfMin = std::numeric_limits<I>::lowest() / 2;

// Factory function for std::mt19937 pseudo-random generator
inline auto createMT19937Generator(unsigned initialSeed = 0) noexcept {
    return std::mt19937(initialSeed != 0 ? initialSeed : std::random_device()());
}

// Constexpr power function a^n
template <class I>
inline constexpr I quickPow(I a, unsigned n) {
    I res = 1;
    for (; n != 0; n >>= 1) {
        if (n & 1) {
            res *= a;
        }
        a *= a;
    }
    return res;
}

// See: https://en.cppreference.com/w/cpp/string/char_traits
struct ci_char_traits : public std::char_traits<char> {
    static char to_upper(char ch) {
        return (char) std::toupper((unsigned char) ch);
    }
    static bool eq(char c1, char c2) {
        return to_upper(c1) == to_upper(c2);
    }
    static bool lt(char c1, char c2) {
        return to_upper(c1) <  to_upper(c2);
    }
    static int compare(const char* s1, const char* s2, std::size_t n) {
        while ( n-- != 0 ) {
            if ( to_upper(*s1) < to_upper(*s2) ) return -1;
            if ( to_upper(*s1) > to_upper(*s2) ) return 1;
            ++s1; ++s2;
        }
        return 0;
    }
    static const char* find(const char* s, std::size_t n, char a) {
        auto const ua (to_upper(a));
        while ( n-- != 0 )
        {
            if (to_upper(*s) == ua)
                return s;
            s++;
        }
        return nullptr;
    }
};

// Case-insensitive string type
using CaseInsensitiveString = std::basic_string<char, ci_char_traits>;

template <class T, class... Args>
concept SameAsOneOf = (std::same_as<T, Args> || ...);

template <class T, class... Args>
concept ConvertibleToOneOf = (std::convertible_to<T, Args> || ...);

template <class T, class... Args>
concept SameAsOneOfWithoutCVRef =
        (std::same_as<std::remove_cvref_t<T>, std::remove_cvref_t<Args>> || ...);

template <class T, class... Args>
concept ConvertibleToOneOfWithoutCVRef =
        (std::convertible_to<std::remove_cvref_t<T>, std::remove_cvref_t<Args>> || ...);

template <class Inst, template <class...> class Tp>
constexpr bool isTemplateInstanceOf = false;

template<template <class...> class Tp, class... Args>
constexpr bool isTemplateInstanceOf<Tp<Args...>, Tp> = true;

template <class Inst, template <class...> class Tp>
concept TemplateInstanceOf = isTemplateInstanceOf<Inst, Tp>;

// Identity of std::string
inline const std::string& toString(const std::string& str) noexcept {
    return str;
}

// Transforms to std::string, specialization to other std::basic_string types
template <class Traits, class Alloc>
inline std::string toString(const std::basic_string<char, Traits, Alloc>& str) noexcept {
    return {str.begin(), str.end()};
}

// Transforms to std::string
inline std::string toString(const char* str) noexcept {
    return {str};
}

// Transforms to std::string
inline std::string toString(char c) noexcept {
    return {1, c};
}

// Transforms integer type to std::string
template <std::integral IntType> requires (!SameAsOneOf<IntType, bool, signed char, unsigned char>)
inline std::string toString(IntType value, int base = 10) noexcept {
    static char buffer[32];
    auto res = std::to_chars(buffer, buffer + sizeof(buffer), value, base);
    return res.ec == std::errc{} ? std::string{buffer, res.ptr} : "(ERROR: std::errc::value_to_large)";
}

// Transforms floating point type to std::string
template <std::floating_point FloatType>
inline std::string toString(FloatType value,
                            std::chars_format fmt = std::chars_format::general) noexcept {
    static char buffer[64];
    auto res = std::to_chars(buffer, buffer + sizeof(buffer), value, fmt);
    return res.ec == std::errc{} ? std::string{buffer, res.ptr} : "(ERROR: std::errc::value_to_large)";
}

// Transforms floating point type to std::string
template <std::floating_point FloatType>
inline std::string toString(FloatType value, std::chars_format fmt, int precision) noexcept {
    static char buffer[64];
    auto res = std::to_chars(buffer, buffer + sizeof(buffer), value, fmt, precision);
    return res.ec == std::errc{} ? std::string{buffer, res.ptr} : "(ERROR: std::errc::value_to_large)";
}

// Transforms floating point type to std::string
template <std::floating_point FloatType, std::same_as<int>... Args> requires (sizeof...(Args) <= 1)
inline std::string toString(FloatType value, char fmt, Args... precision) noexcept {
    using namespace std::string_literals;
    switch (fmt) {
        case 'a': return toString(value, std::chars_format::hex, precision...);
        case 'e': return toString(value, std::chars_format::scientific, precision...);
        case 'f': return toString(value, std::chars_format::fixed, precision...);
        case 'g': return toString(value, std::chars_format::general, precision...);
        default:  return "(ERROR: unspecified format '"s + fmt + "')";
    }
}

template <class T> concept HasToString = requires(std::decay_t<T> x) {
    { toString(x) } -> ConvertibleToOneOfWithoutCVRef<std::string>;
};

// Some other types with std::format are omitted
template <class T>
concept HasStdFormatter = std::is_arithmetic_v<T>
        || std::is_pointer_v<std::decay_t<T>>
        || std::same_as<T, std::nullptr_t>
        || TemplateInstanceOf<T, std::basic_string>
        || TemplateInstanceOf<T, std::basic_string_view>
        || TemplateInstanceOf<T, std::chrono::duration>
        || std::chrono::is_clock_v<T>;

// A general formatter for types that support toString()
template <class T> requires (HasToString<T> && !HasStdFormatter<T>)
struct FORMAT_NAMESPACE::formatter<T>: FORMAT_NAMESPACE::formatter<std::string, char> {
    template<class FormatContext>
    auto format(const T& value, FormatContext& fc) {
        auto str = toString(value);
        return FORMAT_NAMESPACE::formatter<std::string, char>::format(str, fc);
    }
};

// Joins all the values with given delimiter
template <std::ranges::range Range, HasToString Delim, HasToString Head, HasToString Tail>
requires HasToString<std::ranges::range_value_t<Range>>
inline std::string join(Range&& values, const Delim& delim = "", const Head& head = "", const Tail& tail = "") {
    auto res = toString(head);
    for (std::size_t count = 0; const auto& x: values) {
        if (count++ != 0) {
            res += toString(delim);
        }
        res += toString(x);
    }
    res += toString(tail);
    return res;
}

template <class T>
inline std::size_t totalBytesUsed(const std::vector<T>& vec) {
    // .capacity() is used instead of .size() to calculate actual memory allocated
    auto res = sizeof(vec) + vec.capacity() * sizeof(T);
    // For nested vector type like std::vector<std::vector<U>>
    if constexpr(TemplateInstanceOf<T, std::vector>) {
        for (const auto& inner: vec) {
            res += totalBytesUsed(inner);
        }
    }
    return res;
}

// Format:
//  if less than 1KB, simply "n bytes"
//  otherwise, "n bytes = x (Kibi|Mebi|Gibi)Bytes" where x has fixed 3 decimal digits
inline std::string totalBytesUsedToString(std::size_t nBytes, const std::locale& loc = std::locale("")) {
    auto res = format(loc, "{} nBytes", nBytes);
    if (nBytes >= 1024) {
        static const char* units[3] = { "KibiBytes", "MebiBytes", "GibiBytes" };
        double value = (double)nBytes / 1024.0;
        int unitId = 0;

        for (; unitId < 2 && value >= 1024.0; ++unitId) {
            value /= 1024.0;
        }
        res += format(" = {:.3f} {}", value, units[unitId]);
    }
    return res;
}

#endif //GRAPH_UTILS_H
