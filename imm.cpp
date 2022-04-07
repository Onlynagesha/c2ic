#include "imm.h"
#include "Logger.h"
#include "PRRGraph.h"
#include "Timer.h"
#include <numbers>
#include <random>

struct GenerateSamplesResult {
    PRRGraphCollection  prrCollection;
    std::size_t         prrCount{};
};

// ** FOR MONOTONIC & SUB-MODULAR CASES ONLY **
void makeSketchFast(PRRGraphCollection& prrCollection, IMMGraph& graph, PRRGraph& prrGraph,
                    const SeedSet& seeds, std::size_t center) {
    // Gets a PRR-sketch with the specified center
    samplePRRSketch(graph, prrGraph, seeds, center);
    // For monotonic cases, boosting never improves the gain of center node
    //  if center is in Ca state (Ca has the highest gain already)
    if (prrGraph[center].state == NodeState::Ca) {
        return;
    }
    // Calculates each gain(v; prrGraph, center) for v in prrGraph
    // gain is implied as gain(v.centerStateTo) - gain(center.state)
    calculateCenterStateToFast(prrGraph);
    // Adds the PRR-sketch to the collection
    prrCollection.add(prrGraph);
}

// No constraints on monotonicity and sub-modularity
void makeSketchSlow(IMMGraph& graph, PRRGraph& prrGraph, const SeedSet& seeds, std::size_t center) {
    // Gets a PRR-sketch with the specified center
    samplePRRSketch(graph, prrGraph, seeds, center);
    // Calculates each gain(v; prrGraph, center) for v in prrGraph
    // gain is implied as gain(v.centerStateTo) - gain(center.state)
    calculateCenterStateToSlow(prrGraph);
}

// Generate PRR-sketches
auto generateSamples(IMMGraph& graph, const SeedSet& seeds, const AlgorithmArgs& args)
{
    auto LB = double{ 1.0 };
    auto prrCount = std::size_t{ 0 }; 

    auto prrCollection = PRRGraphCollection(args["n"].get<std::size_t>(), seeds);
    
    // Uniformly generates a center node in [0, n) each time
    static auto gen = createMT19937Generator();
    auto distCenter = std::uniform_int_distribution<std::size_t>(0, args["n"].get<std::size_t>() - 1);

    double theta = args["theta0-pr"].get<double>();
    double minS = (1 + ns::sqrt2 * args["epsilon-pr"].get<double>());

    auto prrGraph = PRRGraph({
        {"nodes", graph.nNodes()},
        {"links", graph.nLinks()},
        {"maxIndex", graph.nNodes()}
    });
    auto sampleLimit = args.u["sample-limit"];

    for (int i = 1; i < (int)args.f["log2N"]; i++) {
        // theta(i) = 2^i * theta(0)
        theta *= 2.0;
        // minS(i) = minS(0) / 2^i
        minS /= 2.0;

        for (; prrCount < sampleLimit && prrCount < (std::size_t)theta; ++prrCount) {
            // Uniformly generates a center
            auto center = distCenter(gen);
            makeSketchFast(prrCollection, graph, prrGraph, seeds, center);
        }
        // Stops early if reaches limit
        if (prrCount >= sampleLimit) {
            LOG_INFO(format("Reaches sample limit {}", sampleLimit));
            break;
        }
        // Check with a greedy selection,
        // S = gain of the selected boosted nodes in average of all PRR-sketches
        double S = prrCollection.select(args.u["k"], nullptr) / (double)prrCount;
        LOG_INFO(format("Iteration #{}: theta = {:.0f}, S = {:.7f}, required minimal S = {:.7f}",
                        i, theta, S, minS));

        if (S >= minS) {
            LB = S * (double)args.u["n"] / (1 + ns::sqrt2 * args["epsilon-pr"].get<double>());
            break;
        }
    }

    if (prrCount < sampleLimit) {
        theta = 2.0 * args["n"].get<double>() * (double)std::pow(args.f["alpha"] + args.f["beta"], 2.0)
                / LB / std::pow(args["epsilon-pr"].get<double>(), 2.0);
        LOG_INFO(format("LB = {:.0f}, theta = {:.0f}", LB, theta));
    }
    for (; prrCount < sampleLimit && prrCount < (std::size_t)theta; ++prrCount) {
        // Uniformly generates a center
        auto center = distCenter(gen);
        makeSketchFast(prrCollection, graph, prrGraph, seeds, center);
    }
    return GenerateSamplesResult{
        .prrCollection = std::move(prrCollection),
        .prrCount = prrCount
    };
}

IMMResult PR_IMM(IMMGraph& graph, const SeedSet& seeds, const AlgorithmArgs& args)
{
    // Prepare parameters
    setNodeStateGain(args["lambda"].get<double>());
    setNodeStatePriority(args["priority"].get<NodePriorityProperty>().priority);

    auto res = IMMResult();
    auto timer = Timer();
    auto [prrCollection, prrCount] = generateSamples(graph, seeds, args);

    LOG_INFO(format("PR_IMM: Finished generating PRR-sketches. Time used = {:.3f} sec.",
        timer.elapsed().count()));
    LOG_INFO(format("Dump PRR-sketch collection:\n{}", prrCollection.dump()));

    res.totalGain = prrCollection.select(args.u["k"], std::back_inserter(res.boostedNodes))
        / (double)prrCount * args["n"].get<double>();

    return res; 
}

IMMResult SA_IMM_LB(IMMGraph& graph, const SeedSet& seeds, const AlgorithmArgs& args) {
    // Prepare parameters
    setNodeStateGain(args["lambda"].get<double>());
    setNodeStatePriority(args["priority"].get<NodePriorityProperty>().priority);

    auto prrGraph = PRRGraph({
        {"nodes", graph.nNodes()},
        {"links", graph.nLinks()},
        {"maxIndex", graph.nNodes()}
    });
    auto prrCollection = PRRGraphCollectionSA(graph.nNodes(), args["gain-threshold-sa"].get<double>(), seeds);
    bool usesRandomGreedy = args.cis["algo"] == "sa-rg-imm" || !NodePriorityProperty::current().satisfies("m");

    // Random greedy for non-monotonic cases
    auto theta = std::min(
            (std::size_t)(usesRandomGreedy ? args.f["theta-sa-rg"] : args.f["theta-sa"]),
            args["sample-limit-sa"].get<std::size_t>());
    LOG_INFO(format("usesRandomGreedy = {}, theta = {}", usesRandomGreedy, theta));

    auto timer = Timer();
    auto logPerPercentage = args["log-per-percentage"].get<double>();

    for (std::size_t v = 0; v != graph.nNodes(); v++) {
        // curGainsByBoosted[s] = How much gain to current center node v if s is chosen as one boosted node
        auto curGainsByBoosted = std::vector<double>(graph.nNodes());

        for (std::size_t j = 0; j < theta; j++) {
            makeSketchSlow(graph, prrGraph, seeds, v);
            for (const auto& node: prrGraph.nodes()) {
                curGainsByBoosted[index(node)] += gain(node.centerStateTo) - gain(prrGraph.centerState);
            }
        }
        // Takes the average
        for (auto& c: curGainsByBoosted) {
            c /= std::floor(theta);
        }
        prrCollection.add(v, curGainsByBoosted);

        double r0 = 100.0 * (double)v       / (double)graph.nNodes();
        double r1 = 100.0 * (double)(v + 1) / (double)graph.nNodes();
        if (std::floor(r0 / logPerPercentage) != std::floor(r1 / logPerPercentage)) {
            LOG_INFO(format("SA_IMM_LB: {:.1f}% finished. "
                             "Total gain records added = {}. "
                             "Total time elapsed = {:.3f} sec.",
                             r1 + 1e-6, prrCollection.nTotalRecords(), timer.elapsed().count()));
        }
    }

    LOG_INFO(format("Dump PRR-sketch collection for SA algorithm: {}", prrCollection.dump()));

    auto res = IMMResult{};
    if (usesRandomGreedy) {
        res.totalGain = prrCollection.randomSelect(args.u["k"], std::back_inserter(res.boostedNodes));
    } else {
        res.totalGain = prrCollection.select(args.u["k"], std::back_inserter(res.boostedNodes));
    }

    return res;
}

IMMResult3 SA_IMM(IMMGraph& graph, const SeedSet& seeds, const AlgorithmArgs& args) {
    auto res = IMMResult3{};
    res.labels[0] = "Upper bound";
    res.labels[1] = "Lower bound";
    res.labels[2] = "(Not used)";

    // Upper bound
    LOG_INFO("SA_IMM: Starts upper bound.");
    auto argsUB = args;
    argsUB["priority"].getRef<NodePriorityProperty>().priority = setNodeStatePriority(returnsValue, 3, 0, 1, 2);
    res[0] = PR_IMM(graph, seeds, argsUB);
    LOG_INFO("SA_IMM: Finished upper bound. Result = " + toString(res[0], 4));

    // Lower bound
    auto timer = Timer();
    res[1] = SA_IMM_LB(graph, seeds, args);
    LOG_INFO("SA_IMM: Finished lower bound. Time used = "
            + toString(timer.elapsed().count(), 'f', 3) + " sec. "
            + "Result = " + toString(res[1], 4));

    res.bestIndex = res[0].totalGain > res[1].totalGain ? 0 : 1;
    return res;
}

