//
// Created by Onlynagesha on 2022/4/4.
//

#ifndef DAWNSEEKER_GRAPH_ASSERT_H
#define DAWNSEEKER_GRAPH_ASSERT_H

/*!
 * @file utils/typeinfo.h
 * @author DawnSeeker (onlynagesha@163.com)
 * @brief Helper template types for static_assert(0) in an unexpected if-constexpr branch
 *
 * For any type template parameter T or any non-type template parameter V
 *  (except the internal helper type that you shall not use),
 * AlwaysFalse<T>::value and AlwaysFalseV<V>::value is always false
 *
 * Example see below:
 *
 *      template <class T, int I>
 *      void demo() {
 *          if constexpr (std::is_arithmetic_v<T>) {
 *              // do sth. here
 *          } else {
 *              // The following wrapped false value works
 *              static_assert(utils::AlwaysFalse<T>::value, "T must be arithmetic type");
 *              // So does this one with NTTP
 *              static_assert(utils::AlwaysFalseV<I>::value);
 *              // But this one doesn't
 *              // static_assert(0) // FAIL to compile!
 *          }
 *      }
 */

namespace utils {
    namespace helper {
        struct AlwaysFalseStaticAssertHelper {};
    }

    template <class T> struct AlwaysFalse {
        static constexpr bool value = false;
    };

    template <> struct AlwaysFalse<helper::AlwaysFalseStaticAssertHelper> {
        static constexpr bool value = true;
    };

    template <auto V> struct AlwaysFalseV {
        static constexpr bool value = false;
    };

    template <> struct AlwaysFalseV<helper::AlwaysFalseStaticAssertHelper{}> {
        static constexpr bool value = true;
    };
}

#endif //DAWNSEEKER_GRAPH_ASSERT_H
