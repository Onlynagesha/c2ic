#ifndef DAWNSEEKER_TIMER_H
#define DAWNSEEKER_TIMER_H

#include "global.h"
#include <chrono>

struct Timer {
public:
    using Clock = ch::system_clock;
    using TimePoint = Clock::time_point;
    // In seconds
    using Duration = ch::duration<double>;

private:
    TimePoint timePoint = Clock::now();

public:
    // The timer starts automatically on construction
    Timer() = default; 

    // Restarts the timer
    void restart() {
        timePoint = Clock::now();
    }

    // Returns the std::chrono::duration for time elapsed since it starts
    // The number of seconds (in double) can be obtained by its count() member function
    Duration elapsed() const {
        return ch::duration_cast<Duration>(Clock::now() - timePoint);
    }

    // For customized Duration type
    template <class OtherDuration> requires (!std::same_as<OtherDuration, Duration>)
    OtherDuration elapsed() const {
        return ch::duration_cast<OtherDuration>(Clock::now() - timePoint);
    }
};

#endif //DAWNSEEKER_TIMER_H