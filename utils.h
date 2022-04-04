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
#include <variant>

#include "utils/assert.h"
#include "utils/type_traits.h"

using namespace utils;

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
