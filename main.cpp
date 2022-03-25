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

    auto prrGraphFast = samplePRRSketch(graph, seeds, v);
    LOG_DEBUG("checkGain", "Dump PRR-sketch:");
    for (const auto& link : prrGraphFast.links()) {
        LOG_DEBUG("checkGain", format(
                "  {} -> {}: state = {}", link.from(), link.to(), link.forceGetState()));
    }
    simulateNoBoost(prrGraphFast, seeds);

    auto prrGraphSlow = prrGraphFast;

    calculateGainFast(prrGraphFast, v);
    calculateGainSlow(prrGraphSlow, v);

    LOG_DEBUG("checkGain", "Result of PRR-Sketch with FAST gain computation:");
    for (const auto& node : prrGraphFast.nodes()) {
        LOG_DEBUG("checkGain", format("  index = {}, gain = {:.1f}", node.index(), node.gain));
    }
    LOG_DEBUG("checkGain", "Result of PRR-Sketch with SLOW gain computation:");
    for (const auto& node : prrGraphSlow.nodes()) {
        LOG_DEBUG("checkGain", format("  index = {}, gain = {:.1f}", node.index(), node.gain));
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
        if (prrGraphFast[i].gain != prrGraphSlow[i].gain) {
            ok = false;
            LOG_ERROR("checkGain", format(
                    "Inconsistent gain of index {}: FAST {} vs. SLOW {}",
                    i, prrGraphFast[i].gain, prrGraphSlow[i].gain));
        }
        totalGain += prrGraphFast[i].gain;
    }
    if (ok) { // NOLINT(bugprone-branch-clone)
        LOG_INFO(format("Test #{}: v = {}, PRR-sketch size = ({}, {}), total gain = {:.1f}, time used = {:.3f} sec.",
                        testId, v, prrGraphFast.nNodes(), prrGraphFast.nLinks(), totalGain, timer.elapsed().count()));
    } else {
        LOG_INFO(format("Dump info:\n{}", sout.str()));
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
    for (std::size_t i = 1; i <= count; i++) {
        auto v = (std::size_t)gen() % graph.nNodes();
        samplePRRSketch(graph, prrGraph, seeds, v);
        simulateNoBoost(prrGraph, seeds);
        calculateGainFast(prrGraph, v);

        if (i % 100 == 0) {
            LOG_INFO(format("After iteration #{}: time used = {:.3f} sec.", i, timer.elapsed().count()));
        }
    }
}

int main(int argc, char** argv) {
    logger::Loggers::add(std::make_shared<logger::Logger>("logger", std::clog, logger::LogLevel::Info));

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

    constexpr std::size_t N = 2000;
    testPRRSketch(graph, seeds, N);

    auto timer = Timer();
    constexpr unsigned T = 100;
    bool ok = true;
    for (unsigned i = 1; ok && i <= T; i++) {
        if (!checkGain(graph, seeds, i)) {
            ok = false;
        }
    }
    if (ok) {
        LOG_INFO(format("All {} test cases OK. Total time used: {:.3f} sec.", T, timer.elapsed().count()));
    }

    return 0;
}