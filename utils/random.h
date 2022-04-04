//
// Created by Onlynagesha on 2022/4/4.
//

#ifndef DAWNSEEKER_GRAPH_RANDOM_H
#define DAWNSEEKER_GRAPH_RANDOM_H

/*!
 * @file utils/random.h
 * @author DawnSeeker (onlynagesha@163.com)
 * @brief Helper for random generation
 */

#include <random>

namespace utils {
    /*!
     * @brief Factory function for std::mt19937 generator with given seed (if provided)
     *
     * If seed is left as 0, a random-generated seed is used.
     *
     * @param initialSeed seed of the random generator (default = 0)
     * @return a std::mt19937 object
     */
    inline auto createMT19937Generator(unsigned initialSeed = 0) noexcept {
        return std::mt19937(initialSeed != 0 ? initialSeed : std::random_device()());
    }
}

#endif //DAWNSEEKER_GRAPH_RANDOM_H
