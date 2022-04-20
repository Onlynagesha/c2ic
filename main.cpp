#include "args-v2.h"
#include "global.h"
#include "imm.h"
#include "Logger.h"
#include "PRRGraph.h"

std::string dumpResult(const IMMGraph& graph, IMMResult& immRes) {
    auto res = std::string{};
    for (auto s: immRes.boostedNodes) {
        res += format("Boosted node {}: in-degree = {}, out-degree = {}\n",
                      s, graph.inDegree(s), graph.outDegree(s));
    }
    return res;
}

std::string makeMinimumResult(
        const IMMGraph& graph,
        const std::vector<std::size_t>& boostedNodes,
        const SimResult& simRes) {
    auto res = std::string{};
    for (auto s: boostedNodes) {
        res += format("{} {} {}\n", s, graph.inDegree(s), graph.outDegree(s));
    }
    for (std::size_t i = 0; auto p: {&simRes.withBoosted, &simRes.withoutBoosted, &simRes.diff}) {
        if (i++ != 0) {
            res += '\n';
        }
        res += format("{} {} {}", p->totalGain, p->positiveGain, p->negativeGain);
    }
    return res;
}

std::string makeMinimumResult(const IMMGraph& graph, IMMResult& immRes, const SimResult& simRes) {
    auto res = std::string{};
    res += toString(immRes.totalGain) + '\n';
    res += makeMinimumResult(graph, immRes.boostedNodes, simRes);
    return res;
}

int mainWorker(int argc, char** argv) {
    auto args = prepareProgramArgs(argc, argv);
    auto graph = readGraph(args.s["graphPath"]);
    auto seeds = readSeedSet(args.s["seedSetPath"]);
    prepareDerivedArgs(args, graph.nNodes());
    LOG_INFO("Arguments: " + toString(args));

    auto property = args["priority"].get<NodePriorityProperty>();

    if (args.cis["algo"] == "greedy") {
        auto res = greedy(graph, seeds, args);
        LOG_INFO("Minimum result with greedy:\n" + makeMinimumResult(graph, res.boostedNodes, res.result));
    } else if (args.cis["algo"] == "pagerank" || args.cis["algo"] == "page-rank") {
        auto res = pageRank(graph, seeds, args);
        LOG_INFO("Minimum result with PageRank:\n" + makeMinimumResult(graph, res.boostedNodes, res.result));
    } else if (args.cis["algo"] == "maxdegree" || args.cis["algo"] == "max-degree") {
        auto res = maxDegree(graph, seeds, args);
        LOG_INFO("Minimum result with max-degree:\n" + makeMinimumResult(graph, res.boostedNodes, res.result));
    } else if (args.cis["algo"] == "pr-imm" || property.satisfies("m-s")) {
        auto res = PR_IMM(graph, seeds, args);
        LOG_INFO(format("PR-IMM result: {}", toString(res, 4)));
        LOG_INFO("Details:\n" + dumpResult(graph, res));

        auto simRes = simulate(graph, seeds, res.boostedNodes, args.u["testTimes"]);
        LOG_INFO(format("Simulation result: {}", simRes));
        LOG_INFO(format("Minimal result:\n{}", makeMinimumResult(graph, res, simRes)));
    }
    else if (args.cis["algo"] == "sa-imm" || args.cis["algo"] == "sa-rg-imm" || property.satisfies("nS")) {
        auto res = SA_IMM(graph, seeds, args);
        LOG_INFO("SA-IMM result: " + toString(res, 4, 4));
        for (std::size_t i: {0, 1}) {
            LOG_INFO(format("Details of {}:\n{}", res.labels[0], res[0]));
        }

        for (std::size_t i = 0; i < 3; i++) {
            auto simRes = simulate(graph, seeds, res[i].boostedNodes, args.u["testTimes"]);
            LOG_INFO(format("Simulation result on '{}': {}", res.labels[i], simRes));
        }
    }
    else {
        LOG_ERROR("Failed to match any algorithm!");
    }

    return 0;
}

int main(int argc, char** argv) try {
    // To standard output
    logger::Loggers::add(std::make_shared<logger::Logger>("output", std::cout, logger::LogLevel::Debug));
    return mainWorker(argc, argv);
} catch (std::exception& e) {
    LOG_CRITICAL("Exception caught: "s + e.what());
    LOG_CRITICAL("Abort.");
    return -1;
}