#include "imm.h"
#include "Logger.h"
#include "PRRGraph.h"
#include "Timer.h"
#include <numbers>
#include <random>

struct GenerateSamplesResult { // NOLINT(cppcoreguidelines-pro-type-member-init)
    PRRGraphCollection  prrCollection;
    std::size_t         prrCount;
};

auto generateSamples(IMMGraph& graph, const SeedSet& seeds, const AlgorithmArguments& args)
{
    auto LB = double{ 0 };
    auto prrCount = std::size_t{ 0 }; 

    auto prrCollection = PRRGraphCollection(args.n); 
    
    // Uniformly generates a center node in [0, n) each time
    static auto gen = std::mt19937(std::random_device()());
    auto distCenter = std::uniform_int_distribution<std::size_t>(0, args.n - 1);

    double theta = args.theta0;
    double minS = (1 + ns::sqrt2 * args.epsilon) * args.n; 

    auto makeSketch = [&]() {
        // Uniformly generates a center
        auto center = distCenter(gen);
        // Gets a PRR-sketch with the specified center
        auto prrGraph = samplePRRSketch(graph, seeds, center);
        // Simulates forward without boosting nodes
        simulateNoBoost(prrGraph, seeds);
        // For monotone cases, boosting never improves the gain of center node
        //  if center is in Ca state (Ca has the highest gain already)
        if (prrGraph[center].state != NodeState::Cr) {
            return;
        }
        // Calculates each gain(v; prrGraph, center) for v in prrGraph
        calculateGainFast(prrGraph, center);
        // Adds the PRR-sketch to the collection
        prrCollection.add(prrGraph);
    };

    for (int i = 1; i < (int)args.log2N; i++) {
        // theta(i) = 2^i * theta(0)
        theta *= 2.0;
        // minS(i) = minS(0) / 2^i
        minS /= 2.0;
        LOG_INFO(format("Iteration {}: theta = {:.0f}, minS = {:.0f}", i, theta, minS));

        for (; prrCount < (std::size_t)theta; ++prrCount) {
            makeSketch();
        }

        // Check with a greedy selection
        double S = prrCollection.greedySelect(args.k, nullptr) / prrCount * args.n; 
        LOG_INFO(format("Iteration {}: S = {:.0f}", i, S));
        if (S >= minS) {
            LB = S / (1 + ns::sqrt2 * args.epsilon);
            LOG_INFO(format("LB = {:.0f}", LB));
            break; 
        }
    }

    theta = 2.0 * args.n * std::pow(args.alpha + args.beta, 2.0) 
        / LB / std::pow(args.epsilon, 2.0);
    LOG_INFO(format("theta = {:.0f}", theta));

    for (; prrCount < (std::size_t)theta; ++prrCount) {
        makeSketch();
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

    with(auto timer = Timer()) {
        auto [prrCollection, prrCount] = generateSamples(graph, seeds, args);
        LOG_INFO(format("Finished generating PRR-sketches. Time used = {:.3f} sec.",
            timer.elapsed().count()));
        LOG_INFO(format("Dump PRR-sketch collection:\n{}", prrCollection.dump()));

        res.totalGain = prrCollection.greedySelect(args.k, std::back_inserter(res.boostedNodes))
            / prrCount * args.n;
    }
    return res; 
}