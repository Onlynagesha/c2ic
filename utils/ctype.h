//
// Created by Onlynagesha on 2022/4/22.
//

#ifndef DAWNSEEKER_UTILS_CTYPE_H
#define DAWNSEEKER_UTILS_CTYPE_H

/*!
 * @file utils/ctype.h
 * @author DawnSeeker (onlynagesha@163.com)
 * @brief Helper for Latin-1 character type with constexpr support.
 */

#include <algorithm>

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
     * @brief Checks whether the character c is a space character.
     *
     * Similar to std::isspace (of default locale) in <cctypes>, but with constexpr support.
     *
     * Space characters include the following:
     *   - space ' '            (0x20)
     *   - form feed '\f'       (0x0c)
     *   - line feed '\n'       (0x0a)
     *   - carriage return '\r' (0x0d)
     *   - horizontal tab '\t'  (0x09)
     *   - vertical tab '\v'    (0x0b)
     *
     * @param c The character to check
     * @return A boolean value, whether c is one of the space characters above
     */
    inline constexpr bool isspace(unsigned char c) {
        // Sorted by ASCII code value
        auto spaceChars = "\t\n\v\f\r ";
        auto n = 6;
        // Using binary search method
        return std::ranges::binary_search(spaceChars, spaceChars + 6, c);
    }

    /*!
     * @brief Checks whether the character c is a decimal digit character, that is, '0' to '9'.
     *
     * Similar to std::isdigit in <cctypes>, but with constexpr support.
     *
     * @param c The character to check
     * @return A boolean value, whether c is one of '0' to '9'
     */
    inline constexpr bool isdigit(unsigned char c) {
        return c >= '0' && c <= '9';
    }

    /*!
     * @brief Checks whether the character c is a lower case letter, that is, 'a' to 'z'.
     *
     * Similar to std::islower in <cctypes>, but with constexpr support.
     *
     * @param c The character to check
     * @return A boolean value, whether c is one of 'a' to 'z'.
     */
    inline constexpr bool islower(unsigned char c) {
        return c >= 'a' && c <= 'z';
    }

    /*!
     * @brief Checks whether the character c is an upper case letter, that is, 'A' to 'Z'.
     *
     * Similar to std::isupper in <cctypes>, but with constexpr support.
     *
     * @param c The character to check
     * @return A boolean value, whether c is one of 'A' to 'Z'.
     */
    inline constexpr bool isupper(unsigned char c) {
        return c >= 'A' && c <= 'Z';
    }
}

#endif //DAWNSEEKER_UTILS_CTYPE_H
