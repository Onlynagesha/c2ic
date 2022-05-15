//
// Created by Onlynagesha on 2022/5/13.
//
#include <chrono>
#include <iostream>
#include <random>
#include "thread.h"
#include "utils/Timer.h"

int main() {
    auto gen = std::mt19937{std::random_device{}.operator()()};
    auto distMs = std::uniform_int_distribution<int>(1000, 1500);

    std::size_t count = 0;
    auto func = [&] (std::size_t i) {
        std::cout << std::format("i = {}, count = {}\n", i, count++);
        std::this_thread::sleep_for(std::chrono::milliseconds{distMs(gen)});
    };

    auto timer = utils::Timer{};
    runTaskGroup(std::views::iota(0, 8) | std::views::transform([func](auto j){return func;}),
                 std::views::iota(0, 100));
    std::cout << "Done. Time used in seconds = " << timer.elapsed() << std::endl;

    return 0;
}