#include "argparse/argparse.hpp"
#include "global.h"
#include "imm.h"
#include "Logger.h"
#include <fstream>
#include <vector>

auto checkGain(IMMGraph& graph, unsigned testId) {
    // Seed set
    static auto sa = std::vector<std::size_t>({ 1, 2 });
    static auto sr = std::vector<std::size_t>({ 3, 6 });
    static auto seeds = SeedSet(sa, sr);

    IMMLink::refreshAllStates(testId);

    auto sout = std::ostringstream();
    // Sets the debug logger that writes log message to sout
    auto debugLogger = std::make_shared<logger::Logger>(
            "checkGain", sout, logger::LogLevel::Debug);
    logger::Loggers::add(debugLogger);

    auto prrGraphFast = samplePRRSketch(graph, seeds, 8);
    LOG_INFO("checkGain", "Dump PRR-sketch:");
    for (const auto& link : prrGraphFast.links()) {
        LOG_INFO("checkGain", format(
                "  {} -> {}: state = {}", link.from(), link.to(), link.forceGetState()));
    }
    simulateNoBoost(prrGraphFast, seeds);

    auto prrGraphSlow = prrGraphFast;

    calculateGainFast(prrGraphFast, 8);
    calculateGainSlow(prrGraphSlow, 8);

    LOG_INFO("checkGain", "Result of PRR-Sketch with FAST gain computation:");
    for (const auto& node : prrGraphFast.nodes()) {
        LOG_INFO("checkGain", format("  index = {}, gain = {:.1f}", node.index(), node.gain));
    }
    LOG_INFO("checkGain", "Result of PRR-Sketch with SLOW gain computation:");
    for (const auto& node : prrGraphSlow.nodes()) {
        LOG_INFO("checkGain", format("  index = {}, gain = {:.1f}", node.index(), node.gain));
    }

    // Do checking
    bool ok = true;
    for (size_t i = 0; i != graph.nNodes(); i++) {
        if (prrGraphFast.hasNode(i) ^ prrGraphSlow.hasNode(i)) {
            ok = false;
            LOG_ERROR(format("Inconsistent node existence of index {}", i));
        }
        if (!prrGraphFast.hasNode(i) || !prrGraphSlow.hasNode(i)) {
            continue;
        }
        if (prrGraphFast[i].gain != prrGraphSlow[i].gain) {
            ok = false;
            LOG_ERROR(format(
                    "Inconsistent gain of index {}: FAST {} vs. SLOW {}",
                    i, prrGraphFast[i].gain, prrGraphSlow[i].gain));
        }
    }

    logger::Loggers::remove(debugLogger->id());
    // Returns whether this test succeeds
    return ok;
}

int main(int argc, char** argv) {
    logger::Loggers::add(std::make_shared<logger::Logger>("logger", std::clog, logger::LogLevel::Debug));

    auto args = AlgorithmArguments();
    if (!parseArgs(args, argc, argv)) {
        LOG_CRITICAL("Failed to parse arguments. Abort.");
        return -1;
    }

    auto graph = readGraph(args.graphPath);
    args.updateValues(graph.nNodes());
    args.updateGainAndPriority();

    LOG_INFO(args.dump());

    constexpr unsigned T = 1000;

    bool ok = true;
    for (unsigned i = 1; ok && i <= T; i++) {
        if (!checkGain(graph, i)) {
            ok = false;
        }
    }
    if (ok) {
        LOG_INFO(format("All {} test cases OK.", T));
    }

    return 0;
}