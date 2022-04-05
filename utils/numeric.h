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

#include <cmath>
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
     * For floating-floating conversion, value changes only by the small error from floating point precision.
     *
     * For floating-integer conversion, a precision loss limit is imposed such that $x - [x] < \epsilon$
     * where $[x]$ is x rounded by nearest. NaN and inf are disallowed in this case.
     *
     * Exceptions may be thrown during conversion if one of the following happens:
     *   - overflow or underflow
     *   - too much precision loss from floating to integer (e.g. 10.25 to 10 is disallowed)
     *   - NaN or inf from floating to integer
     *
     * @tparam To The destination type of conversion
     * @param from The value to be converted
     * @param eps Precision loss limit $\epsilon$ (see above, 1e-12 by default)
     * @return Value with the converted type
     * @throws std::out_of_range if underflow or overflow happens
     */
    template <class To, class From> requires (std::is_arithmetic_v<To> && std::is_arithmetic_v<From>)
    inline To value_safe_arithmetic_cast(From from, double eps = 1e-12) {
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
            // For Integer <- Floating
            if constexpr (std::is_integral_v<To> && std::is_floating_point_v<From>) {
                // (1) Check NaN
                if (std::isnan(from)) {
                    throw std::out_of_range("NaN is disallowed in conversion from floating point to integer");
                }
                // (2) Check inf
                if (std::isinf(from)) {
                    throw std::out_of_range("inf is disallowed in conversion from floating point to integer");
                }
                // (3) Check precision loss
                if (std::fabs(std::remainder(from, 1.0)) >= eps) {
                    throw std::out_of_range("Too much precision loss from floating point to integer");
                }
                // The floating value shall be rounded to nearest
                //  to prevent rounding down during explicit conversion below
                from = std::round(from);
            }
            if (from < std::numeric_limits<To>::min() && from > std::numeric_limits<To>::max()) {
                throw std::out_of_range(
                        "Overflow or underflow during arithmetic cast with floating point");
            }
        }

        // If value-safe, gets the value by explicit type conversion
        return static_cast<To>(from);
    }
}

#endif //DAWNSEEKER_GRAPH_NUMERIC_H
