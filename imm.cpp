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

void makeSketch(PRRGraphCollection& prrCollection, IMMGraph& graph, PRRGraph& prrGraph,
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

// Generate PRR-sketches
auto generateSamples(IMMGraph& graph, const SeedSet& seeds, const AlgorithmArguments& args)
{
    auto LB = double{ 1.0 };
    auto prrCount = std::size_t{ 0 }; 

    auto prrCollection = PRRGraphCollection(args.n, seeds);
    
    // Uniformly generates a center node in [0, n) each time
    static auto gen = createMT19937Generator();
    auto distCenter = std::uniform_int_distribution<std::size_t>(0, args.n - 1);

    double theta = args.theta0;
    double minS = (1 + ns::sqrt2 * args.epsilon);

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
            makeSketch(prrCollection, graph, prrGraph, seeds, center);
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
            LB = S * (double)args.n / (1 + ns::sqrt2 * args.epsilon);
            break;
        }
    }

    if (prrCount < args.sampleLimit) {
        theta = 2.0 * (double)args.n * std::pow(args.alpha + args.beta, 2.0)
                / LB / std::pow(args.epsilon, 2.0);
        LOG_INFO(format("LB = {:.0f}, theta = {:.0f}", LB, theta));
    }
    for (; prrCount < args.sampleLimit && prrCount < (std::size_t)theta; ++prrCount) {
        // Uniformly generates a center
        auto center = distCenter(gen);
        makeSketch(prrCollection, graph, prrGraph, seeds, center);
    }
    return GenerateSamplesResult{
        .prrCollection = std::move(prrCollection),
        .prrCount = prrCount
    };
}

IMMResult PR_IMM(IMMGraph& graph, const SeedSet& seeds, AlgorithmArguments args)
{
    // Prepare parameters
    args.ell *= (1.0 + ns::ln2 / args.lnN);
    setNodeStateGain(args.lambda);
    setNodeStatePriority(args.caPlus, args.ca, args.cr, args.crMinus); 

    auto res = IMMResult();
    auto timer = Timer();
    auto [prrCollection, prrCount] = generateSamples(graph, seeds, args);

    LOG_INFO(format("Finished generating PRR-sketches. Time used = {:.3f} sec.",
        timer.elapsed().count()));
    LOG_INFO(format("Dump PRR-sketch collection:\n{}", prrCollection.dump()));

    res.totalGain = prrCollection.select(args.k, std::back_inserter(res.boostedNodes))
        / (double)prrCount * (double)args.n;

    return res; 
}
