#include "args-v2.h"
#include "global.h"
#include "imm.h"
#include "Logger.h"
#include "PRRGraph.h"
#include "simulate.h"

std::string dumpResult(const IMMGraph& graph, IMMResult& immRes) {
    auto res = std::string{};

    auto sumInDeg = std::size_t{0};
    auto sumOutDeg = std::size_t{0};
    auto maxInDeg = std::numeric_limits<std::size_t>::lowest();
    auto maxOutDeg = std::numeric_limits<std::size_t>::lowest();
    auto minInDeg = std::numeric_limits<std::size_t>::max();
    auto minOutDeg = std::numeric_limits<std::size_t>::max();

    for (auto s: immRes.boostedNodes) {
        auto inDeg = graph.fastInDegree(s);
        auto outDeg = graph.fastOutDegree(s);
        res += format("Boosted node {}: in-degree = {}, out-degree = {}\n",
                      s, inDeg, outDeg);
        sumInDeg += inDeg;
        sumOutDeg += outDeg;
        maxInDeg = std::max(maxInDeg, inDeg);
        minInDeg = std::min(minInDeg, inDeg);
        maxOutDeg = std::max(maxOutDeg, outDeg);
        minOutDeg = std::min(minOutDeg, outDeg);
    }

    if (!immRes.boostedNodes.empty()) {
        res += format("In-degree:  max = {}, min = {}, avg = {:.3f}\n",
                      maxInDeg, minInDeg, 1.0 * (double)sumInDeg / (double)immRes.boostedNodes.size());
        res += format("Out-degree: max = {}, min = {}, avg = {:.3f}",
                      maxOutDeg, minOutDeg, 1.0 * (double)sumOutDeg / (double)immRes.boostedNodes.size());
    }

    return res;
}

std::string makeMinimumResult(
        const IMMGraph& graph,
        const std::vector<std::size_t>& boostedNodes,
        const SimResult& simRes) {
    auto res = std::string{};
    // First K lines: Boosted nodes, index, in-degree, out-degree
    for (auto s: boostedNodes) {
        res += format("{} {} {}\n", s, graph.inDegree(s), graph.outDegree(s));
    }
    // Then one line, simulation result (difference between with and without boosted nodes)
    res += format("{} {} {}", simRes.diff.totalGain, simRes.diff.positiveGain, simRes.diff.negativeGain);
    return res;
}

std::string makeMinimumResult(
        const IMMGraph& graph,
        const IMMResult& immRes,
        const std::vector<SimResult>& simResults) {
    auto res = std::string{};
    // First line: total gain during greedy selection process in PR-IMM algorithm
    res += toString(immRes.totalGain) + '\n';
    // Then K lines: Boosted nodes, index, in-degree, out-degree
    for (auto s: immRes.boostedNodes) {
        res += format("{} {} {}\n", s, graph.inDegree(s), graph.outDegree(s));
    }
    // Then K lines: Simulation result (difference between with and without boosted nodes)
    //  using the first i boosted nodes, for i = 1 to K
    // total, positive, negative
    for (std::size_t i = 0; const auto& simRes: simResults) {
        if (i++ != 0) {
            res += '\n';
        }
        res += format("{} {} {}", simRes.diff.totalGain, simRes.diff.positiveGain, simRes.diff.negativeGain);
    }
    return res;
}

auto simulateForEachPrefix(IMMGraph& graph, const SeedSet& seeds, const std::vector<std::size_t>& boostedNodes,
                           std::size_t simTimes, std::size_t nThreads) {
    constexpr std::size_t nSteps = 20;

    auto res = std::vector<SimResult>{};
    auto r = std::pow((double)boostedNodes.size() + 1e-6, 1.0 / (double)(nSteps - 1));
    auto last = std::size_t{0};
    for (std::size_t i = 0; i < nSteps; i++) {
        auto k = static_cast<std::size_t>(std::pow(r, i));
        if (k == last) {
            continue;
        }
        last = k;
        auto simRes = simulate(graph, seeds, boostedNodes | vs::take(k), simTimes, nThreads);
        LOG_INFO(format("Simulation result with first {} boosted nodes: {}", k, simRes));
        res.push_back(simRes);
    }
    return res;
}

auto simulateForEachPrefix(IMMGraph& graph, const SeedSet& seeds, const IMMResult& immRes,
        std::size_t simTimes, std::size_t nThreads) {
    simulateForEachPrefix(graph, seeds, immRes.boostedNodes, simTimes, nThreads);
}

int mainWorker(int argc, char** argv) {
    auto args = prepareProgramArgs(argc, argv);
    auto graph = readGraph(args.s["graphPath"]);
    auto seeds = readSeedSet(args.s["seedSetPath"]);
    prepareDerivedArgs(args, graph.nNodes());
    LOG_INFO("Arguments: " + toString(args));

    auto property = args["priority"].get<NodePriorityProperty>();
    LOG_INFO("Priority array = " + utils::join(property.priority, ", ", "{", "}"));
    LOG_INFO("Priority property: " + property.dump());
    auto nThreads = args["n-threads"].get<std::size_t>();

    if (args.cis["algo"] == "greedy") {
        auto res = greedy(graph, seeds, args);
    } else if (args.cis["algo"] == "pagerank" || args.cis["algo"] == "page-rank") {
        auto res = pageRank(graph, seeds, args);
        auto simResults = simulateForEachPrefix(graph, seeds, res.boostedNodes, args.u["test-times"], nThreads);
    } else if (args.cis["algo"] == "maxdegree" || args.cis["algo"] == "max-degree") {
        auto res = maxDegree(graph, seeds, args);
        auto simResults = simulateForEachPrefix(graph, seeds, res.boostedNodes, args.u["test-times"], nThreads);
    } else if (args.cis["algo"] == "pr-imm" || property.satisfies("m-s")) {
        auto res = PR_IMM(graph, seeds, args);
        LOG_INFO(format("PR-IMM result: {}", toString(res, 4)));
        LOG_INFO("Details:\n" + dumpResult(graph, res));

        auto simResults = simulateForEachPrefix(graph, seeds, res.boostedNodes, args.u["test-times"], nThreads);
        //auto simResult = simulate(graph, seeds, res.boostedNodes, args.u["test-times"], nThreads);
        //LOG_INFO(format("Simulation result with {} boosted nodes: {}", args.u["k"], simResult));
    }
    else if (args.cis["algo"] == "sa-imm" || args.cis["algo"] == "sa-rg-imm" || property.satisfies("nS")) {
        auto res = SA_IMM(graph, seeds, args);
        LOG_INFO("SA-IMM result: " + toString(res, 4, 4));
        for (std::size_t i: {0, 1}) {
            LOG_INFO(format("Details of {}:\n{}", res.labels[i], res[i]));
        }

        for (std::size_t i: {0, 1}) {
            LOG_INFO(format("Starts simulation of {}", res.labels[i]));
            //auto simRes = simulateForEachPrefix(graph, seeds, res[i], args.u["testTimes"], nThreads);
            auto simRes = simulate(graph, seeds, res[i].boostedNodes, args.u["test-times"], nThreads);
            LOG_INFO("Result: " + toString(simRes));
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