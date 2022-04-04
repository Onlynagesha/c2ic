//
// Created by Onlynagesha on 2022/4/4.
//

#ifndef DAWNSEEKER_GRAPH_NUMERIC_H
#define DAWNSEEKER_GRAPH_NUMERIC_H

/*!
 * @file utils/numeric.h
 * @author DawnSeeker (onlynagesha@163.com)
 * @brief Numeric functions and constants
 */

#include <concepts>
#include <limits>
#include <numeric>
#include <stdexcept>

namespace utils {
    /*!
     * @brief Template variable, $+\infty$ of type I
     *
     * Set as MAX / 2 to prevent overflow during adding operation
     *
     * @tparam I
     */
    template <class I> requires std::integral<I> || std::floating_point<I>
    constexpr I halfMax = std::numeric_limits<I>::max() / 2;

    /*!
     * @brief Template variable, $-\infty$ of type I
     *
     * Set as MIN / 2 to prevent underflow during adding operation
     *
     * @tparam I
     */
    template <class I> requires std::integral<I> || std::floating_point<I>
    constexpr I halfMin = std::numeric_limits<I>::lowest() / 2;

    /*!
     * @brief Constexpr quick power of a to n which computes $a^n$ in $O(\log n)$ time
     *
     * @param a
     * @param n
     * @return $a^n$
     */
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

    /*!
     * @brief Value-safe conversion of numbers (including integers and floating points) to specific type
     *
     * It's guaranteed that conversion between integer types never changes the value.
     *
     * For floating point conversion, value changes only by the small error from floating point precision.
     *
     * Exceptions may be thrown during conversion if overflow or underflow happens.
     *
     * @tparam To The destination type of conversion
     * @param from The value to be converted
     * @return Value with the converted type
     * @throws std::out_of_range if underflow or overflow happens
     */
    template <class To, class From> requires (std::is_arithmetic_v<To> && std::is_arithmetic_v<From>)
    inline To value_safe_arithmetic_cast(From from) {
        // If both are integers, use std::in_range for value-safe comparison
        if constexpr (std::is_integral_v<To> && std::is_integral_v<From>) {
            if (!std::in_range<To>(from)) {
                throw std::out_of_range(
                        "Overflow or underflow during arithmetic cast from integer to integer");
            }
        }
        // Otherwise, use implicit conversion for comparison between floating point
        //  and another floating point or an integer
        else {
            if (from < std::numeric_limits<To>::min() && from > std::numeric_limits<To>::max()) {
                throw std::out_of_range(
                        "Overflow or underflow during arithmetic cast without floating point");
            }
        }
        // If value-safe, gets the value by explicit type conversion
        return static_cast<To>(from);
    }
}

#endif //DAWNSEEKER_GRAPH_NUMERIC_H
