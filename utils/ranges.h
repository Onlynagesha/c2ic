//
// Created by Onlynagesha on 2022/4/8.
//

#ifndef DAWNSEEKER_UTILS_RANGES_H
#define DAWNSEEKER_UTILS_RANGES_H

#include <algorithm>
#include <functional>
#include <ranges>
#include <vector>
#include "type_traits.h"

namespace utils::ranges {
    /*!
     * @brief Performs for_each operation on the ranges respectively
     * @param func The function object to be invoked
     * @param ranges The range objects to perform for-each
     */
    template <class Func, std::ranges::input_range... Ranges>
    requires (std::is_invocable_v<Func, std::ranges::range_reference_t<Ranges>> && ...)
    void concatForEach(Func&& func, Ranges&&... ranges) {
        (std::ranges::for_each(ranges, func), ...);
    }
}

#endif //DAWNSEEKER_UTILS_RANGES_H
