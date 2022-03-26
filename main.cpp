#include "argparse/argparse.hpp"
#include "global.h"
#include "imm.h"
#include "Logger.h"
#include "PRRGraph.h"
#include "Timer.h"
#include <fstream>
#include <vector>

auto checkGain(IMMGraph& graph, const SeedSet& seeds, unsigned testId) {
    auto timer = Timer();

    IMMLink::refreshAllStates(testId);
    auto gen = createMT19937Generator(testId);
    auto v = (std::size_t)gen() % graph.nNodes();

    auto sout = std::ostringstream();
    // Sets the debug logger that writes log message to sout
    auto debugLogger = std::make_shared<logger::Logger>(
            "checkGain", sout, logger::LogLevel::Debug);
    logger::Loggers::add(debugLogger);

    LOG_DEBUG(format("center = {}", v));

    auto prrGraphFast = samplePRRSketch(graph, seeds, v);
    LOG_DEBUG("checkGain", "Dump PRR-sketch:");
    for (const auto& link : prrGraphFast.links()) {
        LOG_DEBUG("checkGain", format(
                "  {} -> {}: state = {}", link.from(), link.to(), link.forceGetState()));
    }

    auto prrGraphSlow = prrGraphFast;

    calculateGainFast(prrGraphFast);
    calculateGainSlow(prrGraphSlow);

    LOG_DEBUG("checkGain", "Result of PRR-Sketch with FAST gain computation:");
    for (const auto& node : prrGraphFast.nodes()) {
        LOG_DEBUG("checkGain", format("  index = {}, gain = {:.1f}", node.index(),
                                      gain(node.centerStateTo) - gain(prrGraphFast.centerState)));
    }
    LOG_DEBUG("checkGain", "Result of PRR-Sketch with SLOW gain computation:");
    for (const auto& node : prrGraphSlow.nodes()) {
        LOG_DEBUG("checkGain", format("  index = {}, gain = {:.1f}", node.index(),
                                      gain(node.centerStateTo) - gain(prrGraphSlow.centerState)));
    }

    // Do checking
    bool ok = true;
    double totalGain = 0.0;
    for (size_t i = 0; i != graph.nNodes(); i++) {
        if (prrGraphFast.hasNode(i) ^ prrGraphSlow.hasNode(i)) {
            ok = false;
            LOG_ERROR("checkGain", format("Inconsistent node existence of index {}", i));
        }
        if (!prrGraphFast.hasNode(i) || !prrGraphSlow.hasNode(i)) {
            continue;
        }
        if (gain(prrGraphFast[i].centerStateTo) != gain(prrGraphSlow[i].centerStateTo)) {
            ok = false;
            LOG_ERROR("checkGain", format(
                    "Inconsistent gain of index {}: FAST {} vs. SLOW {}",
                    i, gain(prrGraphFast[i].centerStateTo) - gain(prrGraphFast.centerState),
                    gain(prrGraphSlow[i].centerStateTo) - gain(prrGraphFast.centerState)));
        }
        totalGain += gain(prrGraphFast[i].centerStateTo) - gain(prrGraphFast.centerState);
    }
    if (ok) { // NOLINT(bugprone-branch-clone)
        LOG_INFO(format("Test #{}: v = {}, PRR-sketch size = ({}, {}), total gain = {:.1f}, time used = {:.3f} sec.",
                        testId, v, prrGraphFast.nNodes(), prrGraphFast.nLinks(), totalGain, timer.elapsed().count()));
    } else {
        LOG_INFO(format("Error in test #{}: Dump info:\n{}", testId, sout.str()));
    }

    logger::Loggers::remove(debugLogger->id());
    // Returns whether this test succeeds
    return ok;
}

void testPRRSketch(IMMGraph& graph, const SeedSet& seeds, std::size_t count) {
    IMMLink::refreshAllStates(1);
    auto gen = createMT19937Generator(1);

    auto prrGraph = PRRGraph({
        {"nodes", graph.nNodes()},
        {"links", graph.nLinks()},
        {"maxIndex", graph.nNodes()}
    });

    auto timer = Timer();
    double totalGain = 0.0;

    for (std::size_t i = 1; i <= count; i++) {
        auto v = (std::size_t)gen() % graph.nNodes();
        samplePRRSketch(graph, prrGraph, seeds, v);
        calculateGainFast(prrGraph);

        for (const auto& node: prrGraph.nodes()) {
            totalGain += gain(node.centerStateTo) - gain(prrGraph.centerState);
        }

        if (i % 100 == 0) {
            LOG_INFO(format("After iteration #{}: total gain = {:.1f}, time used = {:.3f} sec.",
                            i, totalGain, timer.elapsed().count()));
        }
    }
}

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

    auto joined = res.boostedNodes
            | vs::transform([](std::size_t x) { return std::to_string(x) + " "; })
            | vs::join
            | vs::common;
    auto nodesStr = std::string(joined.begin(), joined.end());
    LOG_INFO(format("Selected nodes: {}", nodesStr));

//    constexpr std::size_t N = 2000;
//    testPRRSketch(graph, seeds, N);

//    auto timer = Timer();
//    constexpr unsigned T = 1000;
//    bool ok = true;
//    for (unsigned i = 1; ok && i <= T; i++) {
//        if (!checkGain(graph, seeds, i)) {
//            ok = false;
//        }
//    }
//    if (ok) {
//        LOG_INFO(format("All {} test cases OK. Total time used: {:.3f} sec.", T, timer.elapsed().count()));
//    }

    return 0;
}