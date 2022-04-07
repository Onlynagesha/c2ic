//
// Created by Onlynagesha on 2022/4/4.
//

#ifndef DAWNSEEKER_GRAPH_TYPE_TRAITS_H
#define DAWNSEEKER_GRAPH_TYPE_TRAITS_H

/*!
 * @file utils/type_traits.h
 * @author DawnSeeker (onlynagesha@163.com)
 * @brief Type traits and concepts
 */

#include <concepts>
#include <tuple>
#include <variant>
#include <version>

namespace utils {
    namespace helper {
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
        template <class T, class A, class B>
        struct IndexOfTypes<T, std::pair<A, B>>: IndexOfTypes<T, A, B> {};

        // Specialization of std::tuple
        template <class T, class... Args>
        struct IndexOfTypes<T, std::tuple<Args...>>: IndexOfTypes<T, Args...> {};

        // Specialization of std::variant
        template <class T, class... Args>
        struct IndexOfTypes<T, std::variant<Args...>>: IndexOfTypes<T, Args...> {};

        // Without cvref
        template <class T, class... Args>
        struct IndexOfTypesNoCVRef: IndexOfTypes<std::remove_cvref_t<T>, std::remove_cvref_t<Args>...> {};

        // Without cvref, specialization of std::pair
        template <class T, class A, class B>
        struct IndexOfTypesNoCVRef<T, std::pair<A, B>>: IndexOfTypesNoCVRef<T, A, B> {};

        // Without cvref, specialization of std::tuple
        template <class T, class... Args>
        struct IndexOfTypesNoCVRef<T, std::tuple<Args...>>: IndexOfTypesNoCVRef<T, Args...> {};

        // Without cvref, specialization of std::variant
        template <class T, class... Args>
        struct IndexOfTypesNoCVRef<T, std::variant<Args...>>: IndexOfTypesNoCVRef<T, Args...> {};

        template <class T>
        constexpr inline bool isNoUniqueAddress() {
            struct Check {
                char dummy;
                // GCC (11 or later) provides standard no_unique_address support
                // MSVC (19.31 and earlier) provides msvc::no_unique_address instead
                [[no_unique_address]] [[msvc::no_unique_address]] T t;
            };
            return sizeof(Check) == 1;
        }
    }

    /*!
     * @brief Template variable as std::size_t, the index of type T among the following Args...
     *
     * The value is equivalent to the pseudocode below:
     *
     *      for i = 0 to len(Args) - 1
     *          if T == Args[i]
     *              return i
     *      return -1
     *
     * The template has specializations to the following types:
     *   - indexOfTypes<T, std::pair<A, B>> returns 0 if T == A, 1 if T == B, -1 otherwise
     *   - indexOfTypes<T, std::tuple<Args...>> is equivalent to indexOfTypes<T, Args...>
     *   - indexOfTypes<T, std::variant<Args...>> is equivalent to indexOfTypes<T, Args...>
     *
     * @tparam T
     * @tparam Args
     */
    template <class T, class... Args>
    constexpr std::size_t indexOfTypes = helper::IndexOfTypes<T, Args...>::value;

    /*!
     * @brief Template variable as std::size_t, the index of remove_cvref_t<T> among the following Args...
     *
     * Equivalent to indexOfTypes<std::remove_cvref_t<T>, std::remove_cvref_t<Args>...>
     *
     * The template also has specializations for Cont in {std::pair, std::tuple, std::variant}:
     *   - indexOfTypesWithoutCVRef<T, Cont<Args...>> = indexOfTypesWithoutCVRef<T, Args...>
     *
     * @tparam T
     * @tparam Args
     */
    template <class T, class... Args>
    constexpr std::size_t indexOfTypesWithoutCVRef = helper::IndexOfTypesNoCVRef<T, Args...>::value;

    /*!
     * @brief Concept on whether T is exactly the same (i.e. cv-ref should match as well) as one of Args...
     *
     * Specializations see indexOfTypes for details
     *
     * @tparam T
     * @tparam Args
     */
    template <class T, class... Args>
    concept SameAsOneOf = indexOfTypes<T, Args...> != std::size_t(-1);

    /*!
     * @brief Concept on whether T is the same as one of Args... without const/volatile specifiers and references
     *
     * Specializations see indexOfTypesWithoutCVRef for details
     *
     * @tparam T
     * @tparam Args
     */
    template <class T, class... Args>
    concept SameAsOneOfWithoutCVRef = indexOfTypesWithoutCVRef<T, Args...> != std::size_t(-1);

    /*!
     * @brief Concept on whether T can convert to one of Args...
     *
     * @tparam T
     * @tparam Args
     */
    template <class T, class... Args>
    concept ConvertibleToOneOf = (std::convertible_to<T, Args> || ...);

    /*!
     * @brief Concept on whether T can convert to one of Args...
     *
     * All the const/volatile specifiers and references are removed for T and Args...
     *
     * @tparam T
     * @tparam Args
     */
    template <class T, class... Args>
    concept ConvertibleToOneOfWithoutCVRef =
    (std::convertible_to<std::remove_cvref_t<T>, std::remove_cvref_t<Args>> || ...);

    /*!
     * @brief Template variable as bool, whether Inst is an instance of template type Tp<class...>
     *
     * Example:
     *
     *      static_assert(isTemplateInstanceOf<std::string, std::basic_string>);
     *      using Vec = std::pmr::vector<int>;
     *      static_assert(isTemplateInstanceOf<Vec, std::vector>);
     *
     * @tparam Inst
     * @tparam Tp
     */
    template <class Inst, template <class...> class Tp>
    constexpr bool isTemplateInstanceOf = false;

    template<template <class...> class Tp, class... Args>
    constexpr bool isTemplateInstanceOf<Tp<Args...>, Tp> = true;

    /*!
     * @brief Concept on whether Inst is an instance of template type Tp<class...>
     *
     * const/volatile specifiers and references are ignored for Inst.
     *
     * Equivalent to isTemplateInstanceOf<std::remove_cvref_t<Inst>, Tp>
     *
     * @tparam Inst
     * @tparam Tp
     */
    template <class Inst, template <class...> class Tp>
    concept TemplateInstanceOf = isTemplateInstanceOf<std::remove_cvref_t<Inst>, Tp>;

    /*!
     * @brief Template variable as bool, whether C is a C++ built-in character type
     *
     * Built-in character types include:
     *   - char
     *   - signed char
     *   - unsigned char
     *   - wchar_t
     *   - char16_t
     *   - char32_t
     *   - char8_t (if supports)
     *
     * @tparam C
     */
    template <class C>
    constexpr bool isCharacterType = SameAsOneOf<
            C, char, signed char, unsigned char, wchar_t, char16_t, char32_t
#ifdef __cpp_char8_t
            ,char8_t
#endif
            >;

    /*!
     * @brief  Concept on whether C is a C++ built-in character type
     *
     * Equivalent to isCharacterType<C>. See isCharacterType<C> for details.
     *
     * @tparam C
     */
    template <class C>
    concept CharacterType = isCharacterType<C>;

    /*!
     * @brief Concept on whether the type T satisfies [[no_unique_address]] attribute
     *
     * Typically, no_unique_address types are those with no non-static member variables
     *  e.g. function objects (or "functors") like std::less.
     *
     * @tparam T
     */
    template <class T>
    concept NoUniqueAddress = helper::isNoUniqueAddress<T>();
}

#endif //DAWNSEEKER_GRAPH_TYPE_TRAITS_H
