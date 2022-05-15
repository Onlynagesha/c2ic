//
// Created by Onlynagesha on 2022/5/13.
//

#ifndef DAWNSEEKER_PROGRESSCOUNTER_H
#define DAWNSEEKER_PROGRESSCOUNTER_H

#include <algorithm>
#include <Logger.h>
#include "utils/Timer.h"

class ProgressCounter {
    std::string     name;

    std::uint64_t   finished;
    std::uint64_t   total;

    double          logPerPercentage;
    utils::Timer    timer;

public:
    explicit ProgressCounter(std::uint64_t total, double logPerPercentage = 5.0):
    finished(0), total(total), logPerPercentage(logPerPercentage) {}

    ProgressCounter(std::string name, std::uint64_t total, double logPerPercentage = 5.0):
    name(std::move(name)), finished(0), total(total), logPerPercentage(logPerPercentage) {}

    bool increment(std::uint64_t n = 1) {
        auto before = finished;
        finished = std::min(finished + n, total);

        auto r0 = 100.0 * (double)before   / (double)total;
        auto r1 = 100.0 * (double)finished / (double)total;
        auto triggered = std::floor(r0 / logPerPercentage) != std::floor(r1 / logPerPercentage);

        if (triggered) {
            LOG_INFO(format("{}{:.1f}% finished. Time elapsed = {:.3f} sec.",
                            name.empty() ? "" : name + ": ", r1 + 1e-6, timer.elapsed().count()));
        }
        return triggered;
    }
};

#endif //DAWNSEEKER_PROGRESSCOUNTER_H
