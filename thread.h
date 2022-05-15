//
// Created by Onlynagesha on 2022/5/13.
//

#ifndef DAWNSEEKER_GRAPH_THREAD_H
#define DAWNSEEKER_GRAPH_THREAD_H

#include <future>
#include <ranges>
#include <semaphore>

template <class FuncRange, class Arg1Range, class... Args>
inline void runTaskGroup(FuncRange&& funcRange, Arg1Range&& arg1Range, Args&&... restArgs) {
    using FuncType = std::ranges::range_value_t<FuncRange>;
    using Arg1Type = std::ranges::range_value_t<Arg1Range>;

    auto argMutex  = std::mutex{};
    auto iter      = std::ranges::begin(arg1Range);
    auto endIter   = std::ranges::end(arg1Range);

    auto threads = std::vector<std::thread>{};
    for (auto&& func: funcRange) {
        threads.emplace_back([&](auto&& func) {
            auto arg1 = Arg1Type{};

            while (true) {
                {
                    auto lock = std::scoped_lock(argMutex);
                    if (iter == endIter) {
                        break;
                    }
                    arg1 = *iter++;
                }
                func(arg1, restArgs...);
            }
        }, std::forward<FuncType>(func));
    }
    for (auto& t: threads) {
        t.join();
    }
}

#endif //DAWNSEEKER_GRAPH_THREAD_H
