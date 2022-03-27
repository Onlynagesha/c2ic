#ifndef DAWNSEEKER_TIMER_H
#define DAWNSEEKER_TIMER_H

#include <chrono>

struct Timer {
public:
    using Clock = std::chrono::system_clock;
    using TimePoint = Clock::time_point;
    // In seconds
    using Duration = std::chrono::duration<double>;

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
    [[nodiscard]] Duration elapsed() const {
        return std::chrono::duration_cast<Duration>(Clock::now() - timePoint);
    }

    // Returns the std::chrono::duration for time elapsed since it starts, and then restarts
    // Equivalent to .elapsed() and then .restart()
    // The number of seconds (in double) can be obtained by its count() member function
    Duration elapsedR() {
        auto res = elapsed();
        restart();
        return res;
    }

    // For customized Duration type
    template <class OtherDuration> requires (!std::same_as<OtherDuration, Duration>)
    OtherDuration elapsed() const {
        return std::chrono::duration_cast<OtherDuration>(Clock::now() - timePoint);
    }

    // For customized Duration type
    // Equivalent to .elapsed() and then .restart()
    template <class OtherDuration> requires (!std::same_as<OtherDuration, Duration>)
    OtherDuration elapsedR() {
        auto res = elapsed<OtherDuration>();
        restart();
        return res;
    }
};

#endif //DAWNSEEKER_TIMER_H