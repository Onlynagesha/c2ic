//
// Created by Onlynagesha on 2022/4/4.
//

#ifndef DAWNSEEKER_GRAPH_TYPEINFO_H
#define DAWNSEEKER_GRAPH_TYPEINFO_H

/*!
 * @file utils/typeinfo.h
 * @author DawnSeeker(onlynagesha@163.com)
 * @brief Type information support
 */

#include <concepts>
#include <type_traits>
#include <version>
#include "assert.h"
#include "type_traits.h"

namespace utils {
    /*!
     * @brief  Returns the name (manually demangled) of a character type C
     *
     * const/volatile specifiers and reference are ignored
     *
     * @tparam C
     * @return A C-style string of the type name
     */
    template <CharacterType C_>
    inline constexpr const char* characterTypeName() {
        using C = std::remove_cvref_t<C_>;

        if constexpr (std::is_same_v<C, signed char>) {
            return "signed char";
        } else if constexpr (std::is_same_v<C, unsigned char>) {
            return "unsigned char";
        } else if constexpr (std::is_same_v<C, char>) {
            return "char";
        } else if constexpr (std::is_same_v<C, wchar_t>) {
            return "wchar_t";
        } else if constexpr (std::is_same_v<C, char16_t>) {
            return "char16_t";
        } else if constexpr (std::is_same_v<C, char32_t>) {
            return "char32_t";
        }
#ifdef __cpp_char8_t
        else if constexpr (std::is_same_v<C, char8_t>) {
            return "char8_t";
        }
#endif
        else {
            static_assert(AlwaysFalse<C>::value, "Unimplemented or invalid character type");
        }
    }

    /*!
     * @brief Returns the name (manually demangled) of an integer type I
     *
     * bool and character types are considered as integer type as well, same as std::integral.
     *
     * const/volatile specifiers and reference are ignored.
     *
     * @tparam I
     * @return A C-style string of the type name
     */
    template <std::integral I_>
    inline constexpr const char* integralTypeName() {
        using I = std::remove_cvref_t<I_>;

        if constexpr (std::is_same_v<I, bool>) {
            return "bool";
        } else if constexpr (CharacterType<I>) {
            return characterTypeName<I>();
        } else if constexpr (std::is_same_v<I, short>) {
            return "short";
        } else if constexpr (std::is_same_v<I, unsigned short>) {
            return "unsigned short";
        } else if constexpr (std::is_same_v<I, int>) {
            return "int";
        } else if constexpr (std::is_same_v<I, unsigned>) {
            return "unsigned int";
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

    /*!
     * @brief Returns the name (manually demangled) of a floating type F
     *
     * const/volatile specifiers and references are ignored.
     *
     * @tparam F
     * @return A C-style string of the type name.
     */
    template <std::floating_point F_>
    inline constexpr const char* floatingPointTypeName() {
        using F = std::remove_cvref_t<F_>;

        if constexpr (std::is_same_v<F, float>) {
            return "float";
        } else if constexpr (std::is_same_v<F, double>) {
            return "double";
        } else if constexpr (std::is_same_v<F, long double>) {
            return "long double";
        } else {
            static_assert(AlwaysFalse<F>::value, "Unimplemented or invalid floating point type");
        }
    }
}

#endif //DAWNSEEKER_GRAPH_TYPEINFO_H
