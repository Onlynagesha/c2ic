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
auto generateSamples(IMMGraph& graph, const SeedSet& seeds, const AlgorithmArguments& args)
{
    auto LB = double{ 1.0 };
    auto prrCount = std::size_t{ 0 }; 

    auto prrCollection = PRRGraphCollection(args.n, seeds);
    
    // Uniformly generates a center node in [0, n) each time
    static auto gen = createMT19937Generator();
    auto distCenter = std::uniform_int_distribution<std::size_t>(0, args.n - 1);

    double theta = args.theta0_pr;
    double minS = (1 + ns::sqrt2 * args.epsilon_pr);

    auto prrGraph = PRRGraph({
        {"nodes", graph.nNodes()},
        {"links", graph.nLinks()},
        {"maxIndex", graph.nNodes()}
    });

    for (int i = 1; i < (int)args.log2N; i++) {
        // theta(i) = 2^i * theta(0)
        theta *= 2.0;
        // minS(i) = minS(0) / 2^i
        minS /= 2.0;

        for (; prrCount < args.sampleLimit && prrCount < (std::size_t)theta; ++prrCount) {
            // Uniformly generates a center
            auto center = distCenter(gen);
            makeSketchFast(prrCollection, graph, prrGraph, seeds, center);
        }
        // Stops early if reaches limit
        if (prrCount >= args.sampleLimit) {
            LOG_INFO(format("Reaches sample limit {}", args.sampleLimit));
            break;
        }
        // Check with a greedy selection,
        // S = gain of the selected boosted nodes in average of all PRR-sketches
        double S = prrCollection.select(args.k, nullptr) / (double)prrCount;
        LOG_INFO(format("Iteration #{}: theta = {:.0f}, S = {:.7f}, required minimal S = {:.7f}",
                        i, theta, S, minS));

        if (S >= minS) {
            LB = S * (double)args.n / (1 + ns::sqrt2 * args.epsilon_pr);
            break;
        }
    }

    if (prrCount < args.sampleLimit) {
        theta = 2.0 * (double)args.n * std::pow(args.alpha + args.beta, 2.0)
                / LB / std::pow(args.epsilon_pr, 2.0);
        LOG_INFO(format("LB = {:.0f}, theta = {:.0f}", LB, theta));
    }
    for (; prrCount < args.sampleLimit && prrCount < (std::size_t)theta; ++prrCount) {
        // Uniformly generates a center
        auto center = distCenter(gen);
        makeSketchFast(prrCollection, graph, prrGraph, seeds, center);
    }
    return GenerateSamplesResult{
        .prrCollection = std::move(prrCollection),
        .prrCount = prrCount
    };
}

IMMResult PR_IMM(IMMGraph& graph, const SeedSet& seeds, const AlgorithmArguments& args)
{
    // Prepare parameters
    setNodeStateGain(args.lambda);
    setNodeStatePriority(args.caPlus, args.ca, args.cr, args.crMinus); 

    auto res = IMMResult();
    auto timer = Timer();
    auto [prrCollection, prrCount] = generateSamples(graph, seeds, args);

    LOG_INFO(format("PR_IMM: Finished generating PRR-sketches. Time used = {:.3f} sec.",
        timer.elapsed().count()));
    LOG_INFO(format("Dump PRR-sketch collection:\n{}", prrCollection.dump()));

    res.totalGain = prrCollection.select(args.k, std::back_inserter(res.boostedNodes))
        / (double)prrCount * (double)args.n;

    return res; 
}

IMMResult SA_IMM_UB(IMMGraph& graph, const SeedSet& seeds, const AlgorithmArguments& args) {
    // Prepare parameters
    setNodeStateGain(args.lambda);
    setNodeStatePriority(args.caPlus, args.ca, args.cr, args.crMinus);

    auto prrGraph = PRRGraph({
        {"nodes", graph.nNodes()},
        {"links", graph.nLinks()},
        {"maxIndex", graph.nNodes()}
    });
    auto prrCollection = PRRGraphCollectionSA(graph.nNodes(), args.gainThreshold_sa, seeds);

    for (std::size_t v = 0; v != graph.nNodes(); v++) {
        // curGainsByBoosted[s] = How much gain to current center node v if s is chosen as one boosted node
        auto curGainsByBoosted = std::vector<double>(graph.nNodes());

        for (std::size_t j = 0; j < (std::size_t)args.theta_sa; j++) {
            makeSketchSlow(graph, prrGraph, seeds, v);
            for (const auto& node: prrGraph.nodes()) {
                curGainsByBoosted[index(node)] += gain(node.centerStateTo) - gain(prrGraph.centerState);
            }
        }
        // Takes the average
        for (auto& c: curGainsByBoosted) {
            c /= std::floor(args.theta_sa);
        }
        prrCollection.add(v, curGainsByBoosted);
    }

    auto res = IMMResult{};
    res.totalGain = prrCollection.select(args.k, std::back_inserter(res.boostedNodes));
    return res;
}

IMMResult3 SA_IMM(IMMGraph& graph, const SeedSet& seeds, const AlgorithmArguments& args) {
    auto res = IMMResult3{};
    res.labels[0] = "Lower bound";
    res.labels[1] = "Upper bound";
    res.labels[2] = "(Not used)";

    // Lower bound
    LOG_INFO("SA_IMM: Starts lower bound.");
    auto argsLB = args;
    argsLB.setPriority(3, 0, 1, 2); // Ca+ > Cr- > Cr > Ca
    res[0] = PR_IMM(graph, seeds, argsLB);
    LOG_INFO("SA_IMM: Finished lower bound.");

    // Upper bound
    auto timer = Timer();
    res[1] = SA_IMM_UB(graph, seeds, args);
    LOG_INFO("SA_IMM: Finished upper bound. Time used = " + toString(timer.elapsed().count(), 'f', 3) + " sec.");

    res.bestIndex = res[0].totalGain > res[1].totalGain ? 0 : 1;
    return res;
}

