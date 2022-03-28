#include "argparse/argparse.hpp"
#include "global.h"
#include "imm.h"
#include "Logger.h"
#include "PRRGraph.h"

int main(int argc, char** argv) {
    logger::Loggers::add(std::make_shared<logger::Logger>("logger", std::clog, logger::LogLevel::Debug));

    auto args = AlgorithmArguments();
    if (!parseArgs(args, argc, argv)) {
        LOG_CRITICAL("Failed to parse arguments. Abort.");
        return -1;
    }

    auto graph = readGraph(args.graphPath);
    auto seeds = readSeedSet(args.seedSetPath);
    args.updateValues(graph.nNodes());
    args.updateGainAndPriority();
    LOG_INFO(args.dump());

    auto property = NodePriorityProperty::current();
    if (args.algo == "pr-imm" || property.satisfies("m-s")) {
        auto res = PR_IMM(graph, seeds, args);
        LOG_INFO(format("PR-IMM result: {}", toString(res, 4)));

        auto simRes = simulate(graph, seeds, res.boostedNodes, args.testTimes);
        LOG_INFO(format("Simulation result: {}", simRes));
    }
    else if (args.algo == "sa-imm" || args.algo == "sa-rg-imm" || property.satisfies("nS")) {
        auto res = SA_IMM(graph, seeds, args);
        LOG_INFO("SA-IMM result: " + toString(res, 4, 4));

        for (std::size_t i = 0; i < 3; i++) {
            auto simRes = simulate(graph, seeds, res[i].boostedNodes, args.testTimes);
            LOG_INFO(format("Simulation result on '{}': {}", res.labels[i], simRes));
        }
    }
    else {
        LOG_ERROR("Error with NodePriorityProperty::satisfies(): failed to match any algorithm!");
    }

    return 0;
}