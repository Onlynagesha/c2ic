//
// Created by Onlynagesha on 2022/4/4.
//

#ifndef DAWNSEEKER_GRAPH_STRING_H
#define DAWNSEEKER_GRAPH_STRING_H

/*!
 * @file utils/string.h
 * @author DawnSeeker (onlynagesha@163.com)
 * @brief Helper for string manipulations
 */

#include <cctype>
#include <charconv>
#include <iostream>
#include <string>
#include "cstring.h"
#include "type_traits.h"

namespace utils {
    namespace helper {
        template <class T> struct StringToCharTraits {
            using type = T;
        };

        template <class Traits, class Alloc>
        struct StringToCharTraits<std::basic_string<char, Traits, Alloc>> {
            using type = Traits;
        };

        template <class T> struct StringToAlloc {
            using type = T;
        };

        template <class Traits, class Alloc>
        struct StringToAlloc<std::basic_string<char, Traits, Alloc>> {
            using type = Alloc;
        };

        template <class T, class Alloc> struct CharTraitsToString {
            using type = std::basic_string<char, T, Alloc>;
        };

        template <class T, class Alloc1, class Alloc2>
        struct CharTraitsToString<std::basic_string<char, T, Alloc1>, Alloc2> {
            using type = std::basic_string<char, T, Alloc2>;
        };
    }

    /*!
     * @brief Concept on whether T is a string-like object
     *
     * "String-like" including:
     *   - Instances of std::basic_string
     *   - CharT* or const CharT* where CharT is a character type
     *
     * @tparam T
     */
    template <class T>
    concept StringLike = TemplateInstanceOf<T, std::basic_string>
    || CharacterType<std::remove_cvref_t<std::remove_pointer_t<std::decay_t<T>>>>;

    /*!
     * @brief Concept on whether type T has a `toString()` free function returning an instance of std::basic_string.
     *
     * the `toString()` function can be:
     *   - `utils::toString()` (see below)
     *   - `toString()` from other namespaces, found by argument-dependent lookup (ADL)
     *
     * @tparam T
     */
    template <class T>
    concept HasToString = requires(std::decay_t<T> x) {
        { toString(x) } -> TemplateInstanceOf<std::basic_string>;
    };

    /*!
     * @brief Transforms a character traits type to the instance of std::basic_string template
     *
     * Behaves like the pseudocode below:
     *
     *      input T, Alloc
     *      if T is an instance of std::basic_string<char, Traits', Alloc'>
     *          return std::basic_string<char, Traits', Alloc>
     *      else
     *          return std::basic_string<char, T, Alloc>
     *
     * @tparam T
     * @tparam Alloc Allocator for std::basic_string (std::allocator<char>, the same as std::string by default)
     */
    template <class T, class Alloc = std::allocator<char>>
    using CharTraitsToString = typename
            helper::CharTraitsToString<std::remove_cvref_t<T>, std::remove_cvref_t<Alloc>>::type;

    /*!
     * @brief Transforms a std::basic_string instance to its character traits type.
     *
     * Behaves like the pseudocode below:
     *
     *      input T
     *      if T is an instance of std::basic_string<char, Traits, Alloc>
     *          return Traits
     *      else
     *          return T
     */
    template <class T>
    using StringToCharTraits = typename
            helper::StringToCharTraits<std::remove_cvref_t<T>>::type;

    /*!
     * @brief Transforms a std::basic_string instance to its allocator type.
     *
     * Behaves like the pseudocode below:
     *
     *      input T
     *      if T is an instance of std::basic_string<char, Traits, Alloc>
     *          return Alloc
     *      else
     *          return T
     */
    template <class T>
    using StringToAllocType = typename
            helper::StringToAlloc<std::remove_cvref_t<T>>::type;

    /*!
     * @brief Case-insensitive character traits for std::basic_string
     *
     * See: https://en.cppreference.com/w/cpp/string/char_traits
     */
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

    /*!
     * @brief Case-insensitive string type
     *
     * `std::basic_string<char, ci_char_traits, std::allocator<char>>`
     */
    using ci_string = std::basic_string<char, ci_char_traits>;

    /*!
     * @brief Support for stream output for case-insensitive strings like `out << str`
     *
     * @param out
     * @param str
     * @return The reference of out itself (which allows chaining like out << s1 << s2 ...)
     */
    inline std::ostream& operator << (std::ostream& out, const ci_string& str) {
        out.write(str.c_str(), str.length());
        return out;
    }

    /*!
     * @brief Support for stream input for case-insensitive strings like 'in >> str'
     *
     * The behavior is similar to where str is std::string
     *
     * @param in
     * @param str
     * @return The reference of in itself (which allows chaining like in >> s1 >> s2 ...)
     */
    inline std::istream& operator >> (std::istream& in, ci_string& str) {
        str.clear();

        if (in.fail()) {
            return in;
        }
        while (true) {
            auto c = in.get();
            if (in.fail()) {
                return in;
            }
            if (!std::isspace(c)) {
                str.push_back(static_cast<char>(c));
                break;
            }
        }
        while (true) {
            auto c = in.get();
            if (in.fail()) {
                return in;
            }
            if (std::isspace(c)) {
                in.putback(static_cast<char>(c));
                return in;
            } else {
                str.push_back(static_cast<char>(c));
            }
        }
    }

    /*!
     * @brief Converts a string-like object str to C-style string
     *
     * Behaves like the pseudocode below:
     *
     *      input str of type T
     *      if T is const char*
     *          return str
     *      else
     *          return str.data()
     *
     * @param str
     * @return A const char* pointer to the head of the string
     */
    template <StringLike T>
    inline const char* toCString(const T& str) noexcept {
        // char* or const char*: returns itself
        if constexpr (std::same_as<char, std::remove_cvref_t<std::remove_pointer_t<T>>>) {
            return str;
        } else {
            return std::ranges::cdata(str);
        }
    }

    /*!
     * @brief Gets the length of a string-like object.
     *
     * Behaves like the pseudocode below:
     *
     *      input str of type T
     *      if T is const char*
     *          return strlen(str)
     *      else
     *          return str.length()
     *
     * @param str
     * @return Length of the string as std::size_t
     */
    template <StringLike T>
    inline std::size_t stringLength(const T& str) noexcept {
        if constexpr (TemplateInstanceOf<T, std::basic_string>) {
            return str.length();
        } else {
            return cstr::strlen(toCString(str));
        }
    }

    /*!
     * @brief Perform character traits conversion on std::basic_string instances
     *
     * Template parameter To can be either the traits type, or the destination std::basic_string instance.
     *
     * Copy may be elided if the following constraints are all satisfied:
     *   - the argument is a left-value,
     *   - Both char traits satisfies NoUniqueAddress (i.e. no non-static member variables in both traits)
     *   - Both strings have the same allocator type
     *
     * If copy is elided from a left-value reference, a const reference is returned.
     * Otherwise, a right-value is returned.
     *
     * @tparam To Character traits type or destination instance of std::basic_string
     * @param str The input string
     * @return The value or reference of the conversion result
     */
    template <class To, TemplateInstanceOf<std::basic_string> From>
    inline decltype(auto) string_traits_cast(From&& str) noexcept {
        using FromWithoutRef = std::remove_cvref_t<From>;
        using FromTraits = typename FromWithoutRef::traits_type;
        using FromAlloc = typename FromWithoutRef::allocator_type;

        using ToTraits = StringToCharTraits<To>;
        // If a char traits is provided, ToAlloc = FromAlloc
        using ToAlloc = std::conditional_t<
                TemplateInstanceOf<To, std::basic_string>,
                StringToAllocType<To>, FromAlloc
                >;

        if constexpr (std::is_same_v<ToAlloc, FromAlloc> && NoUniqueAddress<FromTraits> && NoUniqueAddress<ToTraits>) {
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

    /*!
     * @brief Parses an integer from a string
     *
     * A generalized (but with different detailed behavior) version of std::stoi, std::stol, std::stoll...
     *
     * Difference including (see: https://en.cppreference.com/w/cpp/utility/from_chars):
     *   - `0x` and `0X` prefixes are not recognized if base is 16
     *   - only the minus sign is recognized (not the plus sign), and only for signed integer types of value
     *   - leading whitespace is not ignored.
     *
     * @tparam T The destination integer type
     * @param str Input string
     * @param pos A std::size_t pointer to where the parsed string length is written, or nullptr.
     * @param base Base of the integer. Valid values are 0, 2, 3 ... 36
     * @return The integer parsed of type T
     * @throw std::out_of_range if the value in the string gets out of range of type T
     * @throw std::invalid_argument if no parsing can be performed
     */
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

    /*!
     * @brief Parses a floating point value from a string
     *
     * A generalized version (but with different detailed behavior) of std::stof, std::stod, std::stold...
     *
     * Difference including (see: https://en.cppreference.com/w/cpp/utility/from_chars):
     *   - the plus sign is not recognized outside of the exponent
     *      (only the minus sign is permitted at the beginning)
     *   - if fmt has `std::chars_format::scientific` set but not `std::chars_format::fixed`,
     *      the exponent part is required (otherwise it is optional)
     *   - if fmt has `std::chars_format::fixed` set but not `std::chars_format::scientific`,
     *      the optional exponent is not permitted
     *   - if fmt is `std::chars_format::hex`, the prefix `0x` or `0X` is not permitted
     *      (the string `0x123` parses as the value `0` with unparsed remainder `x123`)
     *   - leading whitespace is not ignored.
     *
     * @tparam T The destination floating point type
     * @param str Input string
     * @param pos A std::size_t pointer to where the parsed string length is written, or nullptr.
     * @param format Format flags for the string
     * @return The parsed value
     * @throw std::invalid_argument if no parse can be performed
     * @throw std::out_of_range if the value range get out of T
     */
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

    /*!
     * @brief Parses a floating point value from a string
     *
     * See fromString(Str, std::size_t*, std::chars_format) for details.
     *
     * The fmt will be transformed to:
     *   - `a` for `std::chars_format::hex`
     *   - `e` for `std::chars_format::scientific`
     *   - `f` for `std::chars_format::fixed`
     *   - `g` for `std::chars_format::general`
     *   - Others are unrecognized.
     *
     * @tparam T The destination floating point type
     * @param str Input string
     * @param pos A std::size_t pointer to where the parsed string length is written, or nullptr.
     * @param fmt Format flag for the string (see above)
     * @return The parsed value
     * @throw std::invalid_argument if the format flag is invalid, or no parse can be performed
     * @throw std::out_of_range if the value range get out of T
     */
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

    /*!
     * @brief A delegate function for stricter parsing from a string to given value type
     *
     * Performs `fromString<T>(str, args...)`
     *
     * "Stricter" means that trailing characters are also disallowed. Exception will be thrown for this case.
     * The whole string must be parsed, no redundant head, no redundant tail.
     *
     * @param str Input string
     * @param args Arguments for the delegated `fromString(str, args...)` function
     * @return The parsed value
     * @throw std::invalid_argument if there are trailing characters.
     * @throw ... that the delegated may throw
     */
    template <class T, StringLike Str, class... Args>
    T fromStringStrict(const Str& str, Args&&... args) {
        std::size_t parseLen;
        T res = fromString<T>(str, &parseLen, std::forward<Args>(args)...);
        if (parseLen < stringLength(str)) {
            throw std::invalid_argument("Redundant trailing characters");
        }
        return res;
    }

    // Transforms to std::string, specialization to all std::basic_string types including std::string itself
// Returns a const-reference if the argument is a left-value, copied/moved value otherwise

    /*!
     * @brief Performs string conversion between two character traits. No change to contents.
     *
     * Equivalent to `string_traits_cast<T>(str)`. See string_traits_cast for details.
     *
     * @tparam T Either the destination std::basic_string instance or the character traits type
     * @param str Input string
     * @return Output string, either as reference or as value
     */
    template <class T = std::string, TemplateInstanceOf<std::basic_string> From>
    inline decltype(auto) toString(From&& str) noexcept {
        return string_traits_cast<T>(std::forward<From>(str));
    }

    /*!
     * @brief Transforms the C-style string to a std::basic_string instance.
     *
     * @tparam T Either the destination std::basic_string instance or the character traits type
     * @param str Input string
     * @return Output string as instance of std::basic_string
     */
    template <class T = std::string>
    inline auto toString(const char* str) noexcept {
        return CharTraitsToString<T>{str};
    }

    /*!
     * @brief Transforms a single character of type char to a std::basic_string instance
     *
     * @tparam T Either the destination std::basic_string instance or the character traits type
     * @param c Input string
     * @return Output string as instance of std::basic_string
     */
    template <class T = std::string>
    inline auto toString(char c) noexcept {
        return CharTraitsToString<T>{1, c};
    }

    /*!
     * @brief Transforms an integer to a std::basic_string instance.
     *
     * Similar to std::stoi, but no exception will be thrown.
     * For conversion failure, a special error message will be returned.
     *
     * @tparam T Either the destination std::basic_string instance or the character traits type
     * @param value Input value to be transformed
     * @param base Base of the value's string representation, valid in 2 ... 36
     * @return Output string as instance of std::basic_string
     */
    template <class T = std::string, std::integral IntType>
    requires (!CharacterType<IntType>)
    inline auto toString(IntType value, int base = 10) noexcept {
        static char buffer[32];
        auto res = std::to_chars(buffer, buffer + sizeof(buffer), value, base);
        return res.ec == std::errc{} ? CharTraitsToString<T>(buffer, res.ptr) : "(ERROR: std::errc::value_to_large)";
    }

    /*!
     * @brief Transforms a floating point value to a std::basic_string instance
     *
     * Similar to std::stoi, but no exception will be thrown.
     * For conversion failure, a special error message will be returned.
     *
     * @tparam T Either the destination std::basic_string instance or the character traits type
     * @param value Input value to be transformed
     * @param fmt Format flag for the string (see: https://en.cppreference.com/w/cpp/utility/to_chars)
     * @return Output string as instance of std::basic_string
     */
    template <class T = std::string, std::floating_point FloatType>
    inline auto toString(FloatType value,
                         std::chars_format fmt = std::chars_format::general) noexcept {
        static char buffer[64];
        auto res = std::to_chars(buffer, buffer + sizeof(buffer), value, fmt);
        return res.ec == std::errc{} ? CharTraitsToString<T>{buffer, res.ptr} : "(ERROR: std::errc::value_to_large)";
    }

    /*!
     * @brief Transforms a floating point value to a std::basic_string instance with given format and precision
     *
     * See toString(value, fmt) for details.
     *
     * @tparam T Either the destination std::basic_string instance or the character traits type
     * @param value Input value to be transformed
     * @param fmt Format flag for the string (see: https://en.cppreference.com/w/cpp/utility/to_chars)
     * @param precision The precision of string representation
     * @return Output string as instance of std::basic_string
     */
    template <class T = std::string, std::floating_point FloatType>
    inline auto toString(FloatType value, std::chars_format fmt, int precision) noexcept {
        static char buffer[64];
        auto res = std::to_chars(buffer, buffer + sizeof(buffer), value, fmt, precision);
        return res.ec == std::errc{} ? CharTraitsToString<T>{buffer, res.ptr} : "(ERROR: std::errc::value_to_large)";
    }

    /*!
     * @brief Transforms a floating point value to a std::basic_string instance with given format and precision
     *
     * See toString(value, fmt) and toString(value, fmt, precision) for details.
     *
     * The format flag can be one of:
     *   - `a` for `std::chars_format::hex`
     *   - `e` for `std::chars_format::scientific`
     *   - `f` for `std::chars_format::fixed`
     *   - `g` for `std::chars_format::general`
     *   - Others are regarded as error
     *
     * @tparam T Either the destination std::basic_string instance or the character traits type
     * @param value Input value to be transformed
     * @param fmt Format flag for the string (see above)
     * @param precision Optional argument, the precision of string representation
     * @return Output string as instance of std::basic_string
     */
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

    /*!
     * @brief Joins all the values as a std::basic_string instance.
     *
     * All the elements should satisfy the concept HasToString, i.e. has a `toString(x)` function.
     *
     * The result is generated as the form: head, value1, delim, value2, delim ... valueN tail.
     *
     * e.g. values = {1, 2, 3}, delim = `" - "`, head = `"{"`, tail = `"}"`, then the result is `{1 - 2 - 3}`.
     *
     * @tparam T Either the destination std::basic_string instance or the character traits type
     * @param values A range of values, the value type should satisfy HasToString
     * @param delim Delimiter element
     * @param head Head element
     * @param tail Tail element
     * @return The joined string as an instance of std::basic_string
     */
    template <class T = std::string,
            std::ranges::range Range,
            HasToString Delim = const char*,
            HasToString Head  = const char*,
            HasToString Tail  = const char*
    >
    requires HasToString<std::ranges::range_value_t<Range>>
    inline auto join(Range&& values, const Delim& delim = "", const Head& head = "", const Tail& tail = "") {
        auto res = toString<T>(head);
        for (std::size_t count = 0; auto&& x: values) {
            if (count++ != 0) {
                res += toString<T>(delim);
            }
            res += toString<T>(x);
        }
        res += toString<T>(tail);
        return res;
    }
}

#endif //DAWNSEEKER_GRAPH_STRING_H
