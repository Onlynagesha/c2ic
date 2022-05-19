//
// Created by Onlynagesha on 2022/5/8.
//

#include "imm.h"
#include "input.h"
#include "Logger.h"
#include "simulate.h"

// todo: Handle output

void doSimulation(
        IMMGraph&                       graph,
        const SeedSet&                  seeds,
        const std::vector<std::size_t>& boostedNodes,
        const BasicArgs&                args) {
    auto simRes = simulate(graph, seeds, boostedNodes, args.kList, args.testTimes, args.nThreads);
    for (std::size_t i = 0; i != args.kList.size(); i++) {
        LOG_INFO(format("Simulation results with k = {}: {}",
                        args.kList[i], toString(simRes[i], true)));
    }
}

template <class ResultType>
void doSimulation(IMMGraph& graph, const SeedSet& seeds, const ResultType& algoRes, const BasicArgs& args) {
    for (const auto& [nSamples, resItem]: algoRes.items) {
        LOG_INFO(format("Starts simulation for result with {} samples:", nSamples));
        doSimulation(graph, seeds, resItem.boostedNodes, args);
    }
}

int mainWorker(int argc, char** argv) {
    auto [graph, seeds, args] = handleInput(argc, argv);
    LOG_INFO("Overall Arguments:\n" + args->dump());

    if (args->algo == AlgorithmLabel::PR_IMM) {
        auto res = PR_IMM(graph, seeds, *args);
        doSimulation(graph, seeds, res, *args);
    } else if (args->algo == AlgorithmLabel::SA_IMM || args->algo == AlgorithmLabel::SA_RG_IMM) {
        auto res = SA_IMM(graph, seeds, *args);
        for (auto i: {0, 1}) {
            LOG_INFO(format("Starts simulation for the result of label '{}':", res.labels[i]));
            doSimulation(graph, seeds, res[i], *args);
        }
    } else {
        auto res = GreedyResult{};
        if (args->algo == AlgorithmLabel::Greedy) {
            res = greedy(graph, seeds, *args);
        } else if (args->algo == AlgorithmLabel::MaxDegree) {
            res = maxDegree(graph, seeds, *args);
        } else if (args->algo == AlgorithmLabel::PageRank) {
            res = pageRank(graph, seeds, *args);
        } else {
            throw std::logic_error("Unexpected case of algorithm selection: unimplemented or wrong logic");
        }
        LOG_INFO(format("Result of {} algorithm: {}", args->algo, utils::join(res.boostedNodes, ", ", "[", "]")));
        doSimulation(graph, seeds, res.boostedNodes, *args);
    }

    return 0;
}

int main(int argc, char** argv) try {
    // To standard output
    // logger::Loggers::add(std::make_shared<logger::Logger>("output", std::cout, logger::LogLevel::Debug));
    return mainWorker(argc, argv);
} catch (std::exception& e) {
    LOG_CRITICAL("Exception caught: "s + e.what());
    LOG_CRITICAL("Abort.");
    return -1;
}