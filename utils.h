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

template <std::integral I>
inline constexpr const char* integralTypeName(I) {
    return integralTypeName<I>();
}

// Throws an exception if out of range
template <class To, class From> requires (std::is_arithmetic_v<To> && std::is_arithmetic_v<From>)
inline To value_safe_arithmetic_cast(From from) {
    auto res = static_cast<To>(from);
    // Assumes the conversion is always valid if To is floating point type
    if constexpr (std::is_integral_v<To>) {
        // To = Integral, From = Integral
        if constexpr (std::is_integral_v<From>) {
            if (!std::in_range<To>(from)) {
                throw std::out_of_range("Failed during arithmetic cast: Integral <- Integral");
            }
        }
            // To = Integral, From = Floating
        else if constexpr (std::is_floating_point_v<To>) {
            if (from < std::numeric_limits<To>::min() && from > std::numeric_limits<To>::max()) {
                throw std::out_of_range("Failed during arithmetic cast: Integral <- Floating");
            }
        }
    }
    return res;
}

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

namespace cecstr {
    constexpr unsigned char toupper(unsigned char c) {
        return (c >= 'a' && c <= 'z') ? c + 'A' - 'a' : c;
    }

    constexpr unsigned char tolower(unsigned char c) {
        return (c >= 'A' && c <= 'Z') ? c + 'a' - 'A' : c;
    }
}

struct ci_strcmp_t {
    // Compares two strings case-insensitively
    constexpr auto operator ()(const char* A, const char* B) const -> std::strong_ordering {
        for (; *A != '\0' && *B != '\0'; ++A, ++B) {
            int a = cecstr::toupper((unsigned char)(*A));
            int b = cecstr::toupper((unsigned char)(*B));
            if (a != b) {
                return a <=> b;
            }
        }
        // If both ends (*A = *B = '\0'), then returns equal
        // If *A == '\0' but B != '\0', returns less
        // If *A != '\0' but B == '\0', returns greater
        return (unsigned)(*A) <=> (unsigned)(*B);
    }
};

inline constexpr ci_strcmp_t ci_strcmp;

struct ci_less {
    constexpr bool operator () (const char* A, const char* B) const {
        return ci_strcmp(A, B) == std::strong_ordering::less;
    }
};

struct AlwaysFalseStaticAssertHelper {};

template <class T> struct AlwaysFalse {
    static constexpr bool value = false;
};

template <> struct AlwaysFalse<AlwaysFalseStaticAssertHelper> {
    static constexpr bool value = true;
};

template <std::integral I>
inline constexpr const char* integralTypeName() {
    if constexpr (!std::is_same_v<I, std::remove_cvref_t<I>>) {
        return integralTypeName<std::remove_cvref_t<I>>();
    } else {
        if constexpr (std::is_same_v<I, char>) {
            return "char";
        } else if constexpr (std::is_same_v<I, signed char>) {
            return "signed char";
        } else if constexpr (std::is_same_v<I, unsigned char>) {
            return "unsigned char";
        } else if constexpr (std::is_same_v<I, short>) {
            return "short";
        } else if constexpr (std::is_same_v<I, unsigned short>) {
            return "unsigned short";
        } else if constexpr (std::is_same_v<I, int>) {
            return "int";
        } else if constexpr (std::is_same_v<I, unsigned>) {
            return "unsigned";
        } else if constexpr (std::is_same_v<I, long>) {
            return "long";
        } else if constexpr (std::is_same_v<I, unsigned long>) {
            return "unsigned long";
        } else if constexpr (std::is_same_v<I, long long>) {
            return "long long";
        } else if constexpr (std::is_same_v<I, unsigned long long>) {
            return "unsigned long long";
        } else {
            static_assert(AlwaysFalse<I>::value, "Unimplemented or invalid integer type");
        }
    }
}

// Case-insensitive string type
using ci_string = std::basic_string<char, ci_char_traits>;

/*
 * Index of type T among all the candidates. The value equals to the pseudocode below:
 *  for (i, Ti) in enumerate(Args...)
 *      if std::same_as<T, Ti>
 *          return i
 *  return std::size_t(-1)
 */
template <class T, class... Args>
struct IndexOfTypes;

template <class T>
struct IndexOfTypes<T> {
    static constexpr std::size_t getValue() {
        return std::size_t(-1);
    }
    static constexpr std::size_t value = std::size_t(-1);
};

template <class T, class Arg1, class... RestArgs>
struct IndexOfTypes<T, Arg1, RestArgs...> {
    static constexpr std::size_t getValue() {
        if constexpr(std::is_same_v<T, Arg1>) {
            return 0;
        } else {
            auto rest = IndexOfTypes<T, RestArgs...>::getValue();
            return rest == std::size_t(-1) ? std::size_t(-1) : rest + 1;
        }
    }

    static constexpr std::size_t value = getValue();
};

// Specialization of std::pair
template <class T, class... Args>
struct IndexOfTypes<T, std::pair<Args...>>: IndexOfTypes<T, Args...> {};

// Specialization of std::tuple
template <class T, class... Args>
struct IndexOfTypes<T, std::tuple<Args...>>: IndexOfTypes<T, Args...> {};

// Specialization of std::variant
template <class T, class... Args>
struct IndexOfTypes<T, std::variant<Args...>>: IndexOfTypes<T, Args...> {};

template <class T, class... Args>
constexpr std::size_t indexOfTypes = IndexOfTypes<T, Args...>::value;

template <class T, class... Args>
constexpr std::size_t indexOfTypesWithoutCVRef =
        IndexOfTypes<std::remove_cvref_t<T>, std::remove_cvref_t<Args>...>::value;

template <class T, class... Args>
concept SameAsOneOf = indexOfTypes<T, Args...> != std::size_t(-1);

template <class T, class... Args>
concept SameAsOneOfWithoutCVRef = indexOfTypesWithoutCVRef<T, Args...> != std::size_t(-1);

template <class T, class... Args>
concept ConvertibleToOneOf = (std::convertible_to<T, Args> || ...);

template <class T, class... Args>
concept ConvertibleToOneOfWithoutCVRef =
        (std::convertible_to<std::remove_cvref_t<T>, std::remove_cvref_t<Args>> || ...);


template <class Inst, template <class...> class Tp>
constexpr bool isTemplateInstanceOf = false;

template<template <class...> class Tp, class... Args>
constexpr bool isTemplateInstanceOf<Tp<Args...>, Tp> = true;

template <class Inst, template <class...> class Tp>
concept TemplateInstanceOf = isTemplateInstanceOf<std::remove_cvref_t<Inst>, Tp>;

// Either a std::string instance, or const char*
template <class T>
concept StringLike = TemplateInstanceOf<T, std::basic_string>
        || std::is_same_v<char, std::remove_cvref_t<std::remove_pointer_t<std::decay_t<T>>>>;

template <StringLike T>
inline const char* toCString(const T& str) noexcept {
    // char* or const char*: returns itself
    if constexpr (std::same_as<char, std::remove_cvref_t<std::remove_pointer_t<T>>>) {
        return str;
    } else {
        return std::ranges::cdata(str);
    }
}

template <StringLike T>
inline std::size_t stringLength(const T& str) noexcept {
    if constexpr (TemplateInstanceOf<T, std::basic_string>) {
        return str.length();
    } else {
        return strlen(toCString(str));
    }
}

template <class T>
constexpr inline bool isNoUniqueAddress() {
    struct Check {
        char dummy;
        [[no_unique_address]] T t;
    };
    return sizeof(Check) == 1;
}

template <class T>
concept NoUniqueAddress = isNoUniqueAddress<T>();

// Converts std::basic_string by traits
// Template parameter To can be either the traits type, or the destination std::basic_string instance
// Copy may be elided if the following constraints are all satisfied:
//  (1) the argument is a left-value,
//  (2) Both char traits satisfies NoUniqueAddress (i.e. no non-static member variables in both traits)
//  (3) The allocator type is the same
template <class To, TemplateInstanceOf<std::basic_string> From>
inline decltype(auto) string_traits_cast(From&& str) noexcept {
    using FromWithoutRef = std::remove_cvref_t<From>;

    using FromTraits = typename FromWithoutRef::traits_type;
    using ToTraits = std::conditional_t<
            TemplateInstanceOf<To, std::basic_string>, typename To::traits_type, To>;

    using FromAlloc = typename FromWithoutRef::allocator_type;
    // If destination allocator is not implied in std::basic_string template parameters, use the same as From
    using ToAlloc = std::conditional_t<
            TemplateInstanceOf<To, std::basic_string>, typename To::allocator_type, FromAlloc>;

    if constexpr (std::is_same_v<FromAlloc, ToAlloc> && NoUniqueAddress<FromTraits> && NoUniqueAddress<ToTraits>) {
        // todo: is reinterpret_cast safe?
        if constexpr (std::is_lvalue_reference_v<From>) {
            // Const left value reference
            return reinterpret_cast<const std::basic_string<char, ToTraits, ToAlloc>&>(str);
        } else {
            // Moves the right value reference str&&
            return reinterpret_cast<std::basic_string<char, ToTraits, ToAlloc>&&>(str);
        }
    } else {
        // Copies the content
        return std::basic_string<char, ToTraits, ToAlloc>(str.begin(), str.end());
    }
}

template <std::integral T, StringLike Str>
T fromString(const Str& str, std::size_t* pos = nullptr, int base = 10) {
    auto n = stringLength(str);
    auto p = toCString(str);
    T value;

    auto [ptr, ec] = std::from_chars(p, p + n, value, base);
    if (ec == std::errc::invalid_argument) {
        throw std::invalid_argument("Invalid argument during conversion from string to integer");
    } else if (ec == std::errc::result_out_of_range) {
        throw std::out_of_range("Result out of range during conversion from string to integer");
    } else {
        if (pos != nullptr) {
            *pos = ptr - p;
        }
        return value;
    }
}

template <std::floating_point T, StringLike Str>
T fromString(const Str& str, std::size_t* pos = nullptr, std::chars_format format = std::chars_format::general) {
    auto n = stringLength(str);
    auto p = toCString(str);
    T value;

    auto [ptr, ec] = std::from_chars(p, p + n, value, format);
    if (ec == std::errc::invalid_argument) {
        throw std::invalid_argument("Invalid argument during conversion from string to floating point");
    } else if (ec == std::errc::result_out_of_range) {
        throw std::out_of_range("Result out of range during conversion from string to floating point");
    } else {
        if (pos != nullptr) {
            *pos = ptr - p;
        }
        return value;
    }
}

template <std::floating_point T, StringLike Str>
T fromString(const Str& str, std::size_t* pos, char fmt) {
    switch (fmt) {
        case 'a': return fromString<T>(str, pos, std::chars_format::hex);
        case 'e': return fromString<T>(str, pos, std::chars_format::scientific);
        case 'f': return fromString<T>(str, pos, std::chars_format::fixed);
        case 'g': return fromString<T>(str, pos, std::chars_format::general);
        default:  throw std::invalid_argument("Invalid format specifier");
    }
}

// Transforms to std::string, specialization to all std::basic_string types including std::string itself
// Returns a const-reference if the argument is a left-value, copied/moved value otherwise
template <TemplateInstanceOf<std::basic_string> From>
inline decltype(auto) toString(From&& str) noexcept {
    return string_traits_cast<std::string>(std::forward<From>(str));
}

template <class T> using CharTraitsToString = std::conditional_t<
        TemplateInstanceOf<T, std::basic_string>,
        T, std::basic_string<char, T, std::allocator<char>>
        >;

// Transforms to std::string
template <class T = std::string>
inline auto toString(const char* str) noexcept {
    static_assert(TemplateInstanceOf<std::string, std::basic_string>);
    return CharTraitsToString<T>{str};
}

// Transforms to std::string
template <class T = std::string>
inline auto toString(char c) noexcept {
    return CharTraitsToString<T>{1, c};
}

// Transforms integer type to std::string
template <class T = std::string, std::integral IntType>
requires (!SameAsOneOf<IntType, bool, signed char, unsigned char>)
inline auto toString(IntType value, int base = 10) noexcept {
    static char buffer[32];
    auto res = std::to_chars(buffer, buffer + sizeof(buffer), value, base);
    return res.ec == std::errc{} ? CharTraitsToString<T>(buffer, res.ptr) : "(ERROR: std::errc::value_to_large)";
}

// Transforms floating point type to std::string
template <class T = std::string, std::floating_point FloatType>
inline auto toString(FloatType value,
                     std::chars_format fmt = std::chars_format::general) noexcept {
    static char buffer[64];
    auto res = std::to_chars(buffer, buffer + sizeof(buffer), value, fmt);
    return res.ec == std::errc{} ? CharTraitsToString<T>{buffer, res.ptr} : "(ERROR: std::errc::value_to_large)";
}

// Transforms floating point type to std::string
template <class T = std::string, std::floating_point FloatType>
inline auto toString(FloatType value, std::chars_format fmt, int precision) noexcept {
    static char buffer[64];
    auto res = std::to_chars(buffer, buffer + sizeof(buffer), value, fmt, precision);
    return res.ec == std::errc{} ? CharTraitsToString<T>{buffer, res.ptr} : "(ERROR: std::errc::value_to_large)";
}

// Transforms floating point type to std::string
template <class T = std::string, std::floating_point FloatType, std::convertible_to<int>... Args>
requires (sizeof...(Args) <= 1)
inline auto toString(FloatType value, char fmt, Args... precision) noexcept {
    using namespace std::string_literals;
    switch (fmt) {
        case 'a': return toString<T>(value, std::chars_format::hex, precision...);
        case 'e': return toString<T>(value, std::chars_format::scientific, precision...);
        case 'f': return toString<T>(value, std::chars_format::fixed, precision...);
        case 'g': return toString<T>(value, std::chars_format::general, precision...);
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

namespace cecstr {
    constexpr std::size_t strspn(const char* str, const char* delims) {
        const char* cur = str;
        for (const char* di; *cur != '\0'; ++cur) {
            for (di = delims; *di != '\0' && *cur != *di; ++di) {}
            // *cur does not match with any delimiter. Ends here
            if (*di == '\0') {
                break;
            }
        }
        return static_cast<std::size_t>(cur - str);
    }

    // Returns a pointer to the '\0' termination character if no delimiter character found
    // This variant behaves more natural when, for example, splitting strings by delimiters
    constexpr const char* strpbrk_no_null(const char* str, const char* delims) {
        for (const char* di; *str != '\0'; ++str) {
            for (di = delims; *di != '\0' && *str != *di; ++di) {}
            // Current character matches some delimiter
            if (*di != '\0') {
                break;
            }
        }
        return str;
    }

    // Returns nullptr if no delimiter character found
    // Consistent with strpbrk in <string.h>
    constexpr const char* strpbrk(const char* str, const char* delims) {
        auto res = strpbrk_no_null(str, delims);
        return *res == '\0' ? nullptr : res;
    }
}

// func(first, last) or func(str, len) to process the sub-string
template <class Func>
inline constexpr void splitByEither(const char* first, const char* last, const char* delims, Func&& func) {
    // Skips the delimiters at the beginning
    first += cecstr::strspn(first, delims);
    for (; first < last; ) {
        // The token is [first, next)
        auto next = cecstr::strpbrk_no_null(first, delims);
        // Invokes the callable
        if constexpr (requires {func(first, next);}) {
            func(first, next);
        } else if constexpr (requires {func(first, std::size_t{});}) {
            func(first, static_cast<std::size_t>(next - first));
        } else {
            static_assert(AlwaysFalse<Func>::value,
                          "Either func(const char*, const char*) or func(const char*, std::size_t) should be supported!");
        }
        // first <- first', skips delimiters in [next, first')
        first = next + cecstr::strspn(next, delims);
    }
}

// func(first, last) or func(str, len) to process the sub-string
template <class Func>
inline constexpr void splitByEither(const char* str, std::size_t len, const char* delims, Func&& func) {
    splitByEither(str, str + len, delims, std::forward<Func>(func));
}

#endif //GRAPH_UTILS_H
