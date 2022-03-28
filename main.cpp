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

    auto res = PR_IMM(graph, seeds, args);
    LOG_INFO(format("Final gain = {:.3f}", res.totalGain));
    LOG_INFO(join(res.boostedNodes, ", ", "Selected boosted nodes: {", "}"));

    auto simRes = simulate(graph, seeds, res.boostedNodes, args.testTimes);
    LOG_INFO(format("Simulation result: {}", simRes));

    return 0;
}