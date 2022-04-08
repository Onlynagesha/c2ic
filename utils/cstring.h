//
// Created by Onlynagesha on 2022/4/4.
//

#ifndef DAWNSEEKER_UTILS_CSTRING_H
#define DAWNSEEKER_UTILS_CSTRING_H

/*!
 * @file utils/cstring.h
 * @author DawnSeeker (onlynagesha@163.com)
 * @brief Helper for C-style null-terminated string operations with constexpr support
 */

#include <compare>
#include <limits>
#include "assert.h"

namespace utils::cstr {
    /*!
     * @brief Converts a character 'a'-'z' to upper bound, others remain the same.
     *
     * Similar to std::toupper in <cctypes>, but with constexpr support.
     *
     * @param c The character to convert
     * @return The converted character
     */
    inline constexpr unsigned char toupper(unsigned char c) {
        return (c >= 'a' && c <= 'z') ? c + 'A' - 'a' : c;
    }

    /*!
     * @brief Converts a character 'A'-'Z' to lower bound, others remain the same.
     *
     * Similar to std::tolower in <cctypes>, but with constexpr support.
     *
     * @param c The character to convert
     * @return The converted character
     */
    inline constexpr unsigned char tolower(unsigned char c) {
        return (c >= 'A' && c <= 'Z') ? c + 'a' - 'A' : c;
    }

    /*!
     * @brief Gets the length of a C-style string str.
     *
     * Similar to std::strlen in <cstring>, but with constexpr support.
     *
     * @param str
     * @return The length as std::size_t
     */
    inline constexpr std::size_t strlen(const char* str) {
        auto tail = str;
        for (; *tail != '\0'; ++tail) {}
        return static_cast<std::size_t>(tail - str);
    }

    /*!
     * @brief Searches the maximal prefix of str whose characters are all in the character set given.
     *
     * Similar to std::strspn in <cstring>, but with constexpr support.
     *
     * e.g. `strspn("ALICEBOB", "ACEIL")` returns 5 for the prefix `"ALICE"`
     *
     * @param str
     * @param charset The character set
     * @return The length of the prefix as std::size_t
     */
    inline constexpr std::size_t strspn(const char* str, const char* charset) {
        const char* cur = str;
        for (const char* di; *cur != '\0'; ++cur) {
            for (di = charset; *di != '\0' && *cur != *di; ++di) {}
            // *cur does not match with anyone in charset. Ends here
            if (*di == '\0') {
                break;
            }
        }
        return static_cast<std::size_t>(cur - str);
    }

    /*!
     * @brief Searches the maximal prefix of str until any of the delimiter characters given.
     *
     * Constexpr is supported.
     *
     * Difference with std::strpbrk in <cstring> is that this function never returns null pointer.
     * Instead, if no delimiter character found, this function returns the ending position of str.
     *
     * e.g.
     *
     *      const char* str = "Al-Aqsa||Baghdad,Cairo;;Damascus";
     *      const char* delims = ",;:";
     *      int count = 0;
     *      for (const char* head = str, tail; *head != '\0'; ) {
     *          tail = strpbrk_no_null(head, delims);
     *          std::cout << ++count << ": " << std::string{head, tail} << "; ";
     *          if (*tail == '\0') {
     *              break;
     *          } else {
     *              head = tail + 1;
     *          }
     *      }
     *      // Expected output:
     *      // 1. Al-Aqsa; 2. ; 3. Baghdad; 4. Cairo; 5. ; 6. Damascus
     *
     * @param str
     * @param delims A string that contains all the delimiter characters
     * @return The ending position p where [str, p) is the maximal prefix searched.
     */
    inline constexpr const char* strpbrk_no_null(const char* str, const char* delims) {
        for (const char* di; *str != '\0'; ++str) {
            for (di = delims; *di != '\0' && *str != *di; ++di) {}
            // Current character matches some delimiter
            if (*di != '\0') {
                break;
            }
        }
        return str;
    }

    /*!
     * @brief Searches the maximal prefix of str until any of the delimiter characters given.
     *
     * Constexpr is supported.
     *
     * Consistent to std::strpbrk in <cstring>, NULL is returned if no delimiter character found.
     *
     * @param str
     * @param delims A string that contains all the delimiter characters
     * @return The ending position p with *p in delims, or nullptr if no delimiter found.
     */
    inline constexpr const char* strpbrk(const char* str, const char* delims) {
        auto res = strpbrk_no_null(str, delims);
        return *res == '\0' ? nullptr : res;
    }

    /*!
     * @brief The function object type that wraps constexpr case-insensitive C-style string comparison.
     */
    struct ci_strcmp_t {
        /*!
         * @brief Compares two C-style strings A and B case-insensitively, with constexpr support.
         *
         * Both strcmp (2 arguments) and strncmp (3 arguments, plus a length limit) are supported.
         *
         * @param A
         * @param B
         * @param n The maximal number of characters to compare (default: +inf)
         * @return 3-way comparison result as std::strong_ordering
         */
        constexpr auto operator ()(const char* A, const char* B,
                                   std::size_t n = std::numeric_limits<std::size_t>::max()) const {
            for (; *A != '\0' && *B != '\0' && n != 0; ++A, ++B, --n) {
                int a = cstr::toupper((unsigned char)(*A));
                int b = cstr::toupper((unsigned char)(*B));
                if (a != b) {
                    return a <=> b;
                }
            }
            return n == 0 ? std::strong_ordering::equal : (unsigned)(*A) <=> (unsigned)(*B);
        }

        /*!
         * @brief Compares two C-style strings A and B case-insensitively, with constexpr support.
         *
         * Two ranges of characters are provided: [A, lastA) and [B, lastB)
         *
         * @param A
         * @param lastA End position of string A
         * @param B
         * @param lastB End position of string B
         * @return 3-way comparison result as std::strong_ordering
         */
        constexpr auto operator ()(const char* A, const char* lastA,
                const char* B, const char* lastB) const {
            for (; A != lastA && B != lastB; ++A, ++B) {
                int a = cstr::toupper((unsigned char)(*A));
                int b = cstr::toupper((unsigned char)(*B));
                if (a != b) {
                    return a <=> b;
                }
            }
            return (int)(A != lastA) <=> (int)(B != lastB);
        }

        /*!
         * @brief Compares two C-style strings A and B case-insensitively, with constexpr support.
         *
         * @param A
         * @param nA Length of string A
         * @param B
         * @param nB Length of string B
         * @return 3-way comparison result as std::strong_ordering
         */
        constexpr auto operator ()(const char* A, std::size_t nA, const char* B, std::size_t nB) const {
            return operator ()(A, A + nA, B, B + nB);
        }
    };

    /*!
     * @brief The function object instance of constexpr case-insensitive C-style string comparison.
     *
     * See ci_strcmp_t::operator() for details.
     */
    inline constexpr ci_strcmp_t ci_strcmp;

    /*!
     * @brief Compares two C-style strings A and B with at most n characters case-insensitively.
     *
     * Constexpr is supported. See ci_strcmp_t::operator() for details.
     *
     * @param A
     * @param B
     * @param n
     * @return 3-way comparison result as std::strong_ordering
     */
    inline constexpr auto ci_strncmp(const char* A, const char* B, std::size_t n) {
        return ci_strcmp(A, B, n);
    }

    /*!
     * @brief The function object type that wraps constexpr C-style string comparison.
     */
    struct strcmp_t {
        /*!
         * @brief Compares two C-style strings A and B with constexpr support.
         *
         * Both strcmp (2 arguments) and strncmp (3 arguments, plus a length limit) are supported.
         *
         * @param A
         * @param B
         * @param n The maximal number of characters to compare (default: +inf)
         * @return 3-way comparison result as std::strong_ordering
         */
        constexpr auto operator ()(const char* A, const char* B,
                                   std::size_t n = std::numeric_limits<std::size_t>::max()) const {
            for (; *A != '\0' && *B != '\0' && n != 0; ++A, ++B, --n) {
                int a = (unsigned char)(*A);
                int b = (unsigned char)(*B);
                if (a != b) {
                    return a <=> b;
                }
            }
            return n == 0 ? std::strong_ordering::equal : (unsigned)(*A) <=> (unsigned)(*B);
        }
    };

    /*!
     * @brief The function object instance of constexpr C-style string comparison.
     *
     * See strcmp_t::operator() for details.
     */
    inline constexpr strcmp_t strcmp;

    /*!
     * @brief The function object that wraps constexpr C-style string comparison
     */
    struct ci_less {
        /*!
         * @brief Compares the two C-style strings A and B, with constexpr support.
         *
         * Similar to std::strcmp in <cstring>, but the return type is std::strong_ordering
         *
         * @param A
         * @param B
         * @return 3-way comparison result as std::strong_ordering
         */
        constexpr bool operator () (const char* A, const char* B) const {
            return ci_strcmp(A, B) == std::strong_ordering::less;
        }
    };

    /*!
     * @brief Performs string split with given delimiter set, with a function handling the substrings.
     *
     * Continuous delimiters are merged as one. Delimiter prefix and suffix are removed.
     * In other words, no empty string will be produces as result.
     *
     * For each substring [s, s+n) split, the callback func(s, s+n) or func(s, n) will be invoked.
     * Either of the two methods should be supported by func. If both supported, take the first (s, s+n) one.
     *
     * e.g.
     *
     *      const char* str = "Al-Aqsa||Baghdad,Cairo;;Damascus||";
     *      int count = 0;
     *      splitByEither(str, str + strlen(str), ",;|", [&](const char* head, const char* tail) {
     *          std::cout << ++count << ": " << std::string{head, tail} << "; ";
     *      });
     *      // Expected output:
     *      // 1. Al-Aqsa; 2. Baghdad; 3. Cairo; 4. Damascus;
     *
     * @param first Head position of the input string [first, last)
     * @param last Tail position of the input string [first, last)
     * @param delims A string contains all the delimiter characters
     * @param func The function object as callback
     */
    template <class Func>
    inline constexpr void splitByEither(const char* first, const char* last, const char* delims, Func&& func) {
        // Skips the delimiters at the beginning
        first += cstr::strspn(first, delims);
        for (; first < last; ) {
            // The token is [first, next)
            auto next = cstr::strpbrk_no_null(first, delims);
            // Invokes the callable
            if constexpr (requires {func(first, next);}) {
                func(first, next);
            } else if constexpr (requires {func(first, std::size_t{});}) {
                func(first, static_cast<std::size_t>(next - first));
            } else {
                static_assert(AlwaysFalse<Func>::value,
                              "Either func(const char*, const char*) "
                              "or func(const char*, std::size_t) should be supported!");
            }
            // first <- first', skips delimiters in [next, first')
            first = next + cstr::strspn(next, delims);
        }
    }

    /*!
     * @brief Performs string split with given delimiter set, with a function handling the substrings.
     *
     * See splitByEither(str, str + len, delims, func) for details.
     *
     * @param str The input string
     * @param len Length of the input string
     * @param delims A string contains all the delimiter characters
     * @param func The function object as callback
     */
    template <class Func>
    inline constexpr void splitByEither(const char* str, std::size_t len, const char* delims, Func&& func) {
        splitByEither(str, str + len, delims, std::forward<Func>(func));
    }
}

#endif //DAWNSEEKER_UTILS_CSTRING_H
