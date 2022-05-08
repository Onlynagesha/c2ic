//
// Created by Onlynagesha on 2022/5/7.
//

#ifndef DAWNSEEKER_UTILS_TIMER_H
#define DAWNSEEKER_UTILS_TIMER_H

/*!
 * @file utils/Timer.h
 * @author DawnSeeker (onlynagesha@163.com)
 * @brief A lightweight timer based on system clock
 */

#include <chrono>

namespace utils {
    struct Timer {
    public:
        using Clock = std::chrono::system_clock;
        using TimePoint = Clock::time_point;
        // In seconds
        using Duration = std::chrono::duration<double>;

    private:
        TimePoint timePoint = Clock::now();

    public:
        /*!
         * @brief Timer starts automatically on construction.
         */
        Timer() = default;

        /*!
         * @brief Restarts the timer.
         */
        void restart() {
            timePoint = Clock::now();
        }

        /*!
         * @brief Gets the time elapsed since the clock starts.
         *
         * To get time elapsed in seconds (as a floating point value), call elapsed().count().
         *
         * @return A std::chrono::duration object, time elapsed (in seconds).
         */
        [[nodiscard]] Duration elapsed() const {
            return std::chrono::duration_cast<Duration>(Clock::now() - timePoint);
        }

        /*!
         * @brief Gets the time elapsed since the clock starts, and then restarts.
         *
         * Equivalent to calling .elapsed() and then .restart(). See elapsed() for details.
         *
         * @return A std::chrono::duration object, time elapsed (in seconds).
         */
        Duration elapsedR() {
            auto res = elapsed();
            restart();
            return res;
        }

        /*!
         * @brief Gets the time elapsed since the clock starts, with duration conversion.
         *
         * Equivalent to std::chrono::duration_cast<OtherDuration>(elapsed()).
         *
         * @tparam OtherDuration The duration type that the result converts to
         * @return An OtherDuration object, time elapsed.
         */
        template <class OtherDuration> requires (!std::same_as<OtherDuration, Duration>)
        OtherDuration elapsed() const {
            return std::chrono::duration_cast<OtherDuration>(Clock::now() - timePoint);
        }

        /*!
         * @brief Gets the time elapsed and then restarts, with duration conversion.
         *
         * Equivalent to std::chrono::duration_cast<OtherDuration>(elapsedR()).
         *
         * @tparam OtherDuration The duration type that the result converts to
         * @return An OtherDuration object, time elapsed.
         */
        template <class OtherDuration> requires (!std::same_as<OtherDuration, Duration>)
        OtherDuration elapsedR() {
            auto res = elapsed<OtherDuration>();
            restart();
            return res;
        }
    };
}

#endif //DAWNSEEKER_UTILS_TIMER_H
