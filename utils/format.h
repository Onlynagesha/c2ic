//
// Created by Onlynagesha on 2022/4/4.
//

#ifndef DAWNSEEKER_UTILS_FORMAT_H
#define DAWNSEEKER_UTILS_FORMAT_H

/*!
 * @file utils/format.h
 * @author DawnSeeker (onlynagesha@163.com)
 * @brief Support for std::format or fmt::format
 *
 * Generates std::formatter or fmt::formatter for every type that supports toString(x) function
 */

#include <chrono>
#include <concepts>
#include <string_view>
#include "string.h"
#include "type_traits.h"

#ifdef __cpp_lib_format
#include <format>
#define DAWNSEEKER_GRAPH_FORMAT_H_ENABLE_CONTENTS
#define DAWNSEEKER_GRAPH_FORMAT_H_FORMATTER_NAMESPACE std
#elif __has_include(<fmt/format.h>)
#include <fmt/format.h>
#define DAWNSEEKER_GRAPH_FORMAT_H_ENABLE_CONTENTS
#define DAWNSEEKER_GRAPH_FORMAT_H_FORMATTER_NAMESPACE fmt
#endif

namespace utils {
    /*!
     * @brief Concept on whether a type has std::formatter specialization provided by C++ standard library.
     *
     * WARNING: This concept does not include all the types in <chrono> (e.g. time zone, etc.)
     * For the whole list of formatter-available types,
     * see: https://en.cppreference.com/w/cpp/utility/format/formatter
     *
     * @tparam T
     */
    template <class T>
    concept HasStdFormatter = std::is_arithmetic_v<T>
    || std::is_pointer_v<std::decay_t<T>>
    || std::same_as<T, std::nullptr_t>
    || TemplateInstanceOf<T, std::basic_string>
    || TemplateInstanceOf<T, std::basic_string_view>
    || TemplateInstanceOf<T, std::chrono::duration>
    || std::chrono::is_clock_v<T>;
}

#ifdef DAWNSEEKER_GRAPH_FORMAT_H_ENABLE_CONTENTS
/*!
 * @brief Generalized support for types that has toString(x) but no std::formatter or fmt::formatter specialization
 *
 * To enable this formatter, the type T should:
 *   - has a toString(x) free function in the same namespace of T
 *   - not be a type to which std::formatter or fmt::formatter has already provided support
 *
 * @tparam T
 */
template <utils::HasToString T> requires (!utils::HasStdFormatter<T>)
struct DAWNSEEKER_GRAPH_FORMAT_H_FORMATTER_NAMESPACE::formatter<T>:
        DAWNSEEKER_GRAPH_FORMAT_H_FORMATTER_NAMESPACE::formatter<std::string, char> {
    template<class FormatContext>
    auto format(const T& value, FormatContext& fc) {
        auto str = toString(value);
        return DAWNSEEKER_GRAPH_FORMAT_H_FORMATTER_NAMESPACE::formatter<std::string, char>::format(str, fc);
    }
};
#endif

#undef DAWNSEEKER_GRAPH_FORMAT_H_ENABLE_CONTENTS
#undef DAWNSEEKER_GRAPH_FORMAT_H_FORMATTER_NAMESPACE

#endif //DAWNSEEKER_UTILS_FORMAT_H
