//
// Created by Onlynagesha on 2022/4/4.
//

#ifndef DAWNSEEKER_UTILS_MISC_H
#define DAWNSEEKER_UTILS_MISC_H

/*!
 * @file utils/misc.h
 * @author DawnSeeker (onlynagesha@163.com)
 * @brief Miscellaneous uncategorized utility
 */

#include <vector>
#include "string.h"
#include "type_traits.h"

namespace utils {
    /*!
     * @brief Gets the total used size in bytes of a std::vector
     *
     * `vec.capacity()`, instead of `vec.size()`, is used for the actual memory usage.
     *
     * WARNING: Only nested std::vector<> is specially handled for type T.
     * For other types, only sizeof(T) is counted.
     * This may produce inaccurate results for nested data structure type.
     *
     * e.g. For `std::vector<std::vector<int>>`, this function returns the true size
     * (i.e. including all the int elements inside),
     * but `std::vector<std::list<int>>` fails to work well.
     *
     * @tparam T
     * @param vec The input vector to count its size
     * @return Size in bytes
     */
    template <class T>
    inline std::size_t totalBytesUsed(const std::vector<T>& vec) {
        // .capacity() is used instead of .size() to calculate actual memory allocated
        auto res = sizeof(vec);
        if constexpr (std::is_same_v<std::remove_cvref_t<T>, bool>) {
            // For vector<bool>, 1 byte contains 8 elements
            res += vec.capacity() / 8;
        } else {
            res += vec.capacity() * sizeof(T);
        }
        // For nested vector type like std::vector<std::vector<U>>
        if constexpr(TemplateInstanceOf<T, std::vector>) {
            res -= vec.size() * sizeof(T);
            for (const auto& inner: vec) {
                res += totalBytesUsed(inner);
            }
        }
        return res;
    }

    /*!
     * @brief Gets the string representation of size in bytes
     *
     * Format:
     *   - if less than 1KB, simply `n bytes` where `n` will be replaced by nBytes
     *   - otherwise, `n bytes = x (Kibi|Mebi|Gibi)Bytes` where `x` has fixed 3 decimal digits
     *
     * @param nBytes
     * @return The string representation as std::string
     */
    inline std::string totalBytesUsedToString(std::size_t nBytes) {
        auto res = toString(nBytes) + " bytes";
        if (nBytes >= 1024) {
            static const char* units[3] = { "KibiBytes", "MebiBytes", "GibiBytes" };
            double value = (double)nBytes / 1024.0;
            int unitId = 0;

            for (; unitId < 2 && value >= 1024.0; ++unitId) {
                value /= 1024.0;
            }
            res += " = " + toString(value, 'f', 3) + ' ' + units[unitId];
        }
        return res;
    }
}


#endif //DAWNSEEKER_UTILS_MISC_H
