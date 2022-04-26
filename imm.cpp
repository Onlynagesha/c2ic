#include "graph/pagerank.h"
#include "imm.h"
#include "Logger.h"
#include "PRRGraph.h"
#include "simulate.h"
#include "Timer.h"
#include <concepts>
#include <future>
#include <numbers>
#include <random>

struct GenerateSamplesResult {
    PRRGraphCollection  prrCollection;
    std::size_t         prrCount{};
};

/*!
 * @brief Generates one PRR-sketch. For monotonic & submodular cases only.
 *
 * The result will be written to prrGraph object, and added to prrCollection object.
 *
 * @param prrCollection The PRR-sketch collection object where the result is added
 * @param graph The whole graph
 * @param prrGraph The PRR-sketch object where the result is written
 * @param seeds The seed set
 * @param center The center node selected
 */
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

/*!
 * @brief Generates R PRR-sketches with multi-threading support. For monotonic & submodular cases only.
 *
 * The results will be written to prrCollection object.
 *
 * @param prrCollection The PRR-sketch collection object where the results are written
 * @param graph The whole graph
 * @param seeds The seed set
 * @param nSamples Number of samples to generate
 * @param nThreads Number of threads to use
 */
void makeSketchesFast(PRRGraphCollection& prrCollection, IMMGraph& graph,
                      const SeedSet& seeds, std::size_t nSamples, std::size_t nThreads) {
    // nSamplesHere = Number of samples dispatched for this thread
    auto func = [&](std::size_t nSamplesHere) {
        auto collection = PRRGraphCollection(graph.nNodes(), seeds);
        auto prrGraph = PRRGraph{{
                {"nodes", graph.nNodes()},
                {"links", graph.nLinks()},
                {"maxIndex", graph.nNodes()}
        }};

        // Uniformly generates a center node in [0, n) each time
        static auto gen = createMT19937Generator();
        auto distCenter = std::uniform_int_distribution<std::size_t>(0, graph.nNodes() - 1);

        for (std::size_t i = 0; i < nSamplesHere; i++) {
            makeSketchFast(collection, graph, prrGraph, seeds, distCenter(gen));
        }
        return collection;
    };

    auto tasks = std::vector<std::future<PRRGraphCollection>>();
    for (std::size_t i = 0; i < nThreads; i++) {
        // Expected PRR-sketch index range = [first, last)
        std::size_t first = nSamples * i / nThreads;
        std::size_t last = nSamples * (i + 1) / nThreads;
        // Dispatches the task to generate (last - first) samples
        tasks.emplace_back(std::async(std::launch::async, func, static_cast<std::size_t>(last - first)));
    }

    // Merges all the results
    for (std::size_t i = 0; i < nThreads; i++) {
        prrCollection.merge(tasks[i].get());
    }
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
auto generateSamples(IMMGraph& graph, const SeedSet& seeds, const ProgramArgs& args)
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
    auto nThreads = args.u["n-threads"];

    for (int i = 1; i < (int)args.f["log2N"]; i++) {
        // theta(i) = 2^i * theta(0)
        theta *= 2.0;
        // minS(i) = minS(0) / 2^i
        minS /= 2.0;

        makeSketchesFast(prrCollection, graph, seeds, (std::size_t)theta - prrCount, nThreads);
        prrCount = (std::size_t)theta;

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

auto generateSamplesFixed(IMMGraph& graph, const SeedSet& seeds, const ProgramArgs& args) {
    auto prrCollection = PRRGraphCollection(args["n"].get<std::size_t>(), seeds);

    // Uniformly generates a center node in [0, n) each time
    static auto gen = createMT19937Generator();
    auto distCenter = std::uniform_int_distribution<std::size_t>(0, args["n"].get<std::size_t>() - 1);

    auto sampleLimit = args["sample-limit"].get<std::size_t>();
    auto nThreads = args["n-threads"].get<std::size_t>();

    auto prrGraph = PRRGraph({
        {"nodes", graph.nNodes()},
        {"links", graph.nLinks()},
        {"maxIndex", graph.nNodes()}
    });

    for (std::size_t iter = 1; iter <= 20; iter++) {
        makeSketchesFast(prrCollection, graph, seeds, sampleLimit, nThreads);

        // Check with a greedy selection & forward simulation
        auto selected = std::vector<std::size_t>();
        prrCollection.select(args.u["k"], std::back_inserter(selected));

        auto simResult = simulate(graph, seeds, selected, args.u["test-times"]);
        LOG_INFO(format("Simulation result after {} samples: {}", sampleLimit * iter, simResult));
    }

    return GenerateSamplesResult{
        .prrCollection = std::move(prrCollection),
        .prrCount = 20 * sampleLimit
    };
}

IMMResult PR_IMM(IMMGraph& graph, const SeedSet& seeds, const ProgramArgs& args)
{
    // Prepare parameters
    setNodeStateGain(args["lambda"].get<double>());
    setNodeStatePriority(args["priority"].get<NodePriorityProperty>().priority);

    auto res = IMMResult();
    auto timer = Timer();
    //auto [prrCollection, prrCount] = generateSamples(graph, seeds, args);
    auto [prrCollection, prrCount] = generateSamplesFixed(graph, seeds, args);

    LOG_INFO(format("PR_IMM: Finished generating PRR-sketches. Time used = {:.3f} sec.",
        timer.elapsed().count()));
    LOG_INFO(format("Dump PRR-sketch collection:\n{}", prrCollection.dump()));

    res.totalGain = prrCollection.select(args.u["k"], std::back_inserter(res.boostedNodes))
        / (double)prrCount * args["n"].get<double>();

    return res; 
}

IMMResult SA_IMM_LB(IMMGraph& graph, const SeedSet& seeds, const ProgramArgs& args) {
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
    auto theta = (std::size_t)std::min(
            (usesRandomGreedy ? args.f["theta-sa-rg"] : args.f["theta-sa"]),
            args.f["sample-limit-sa"]);
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
            c /= (double)theta;
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

IMMResult3 SA_IMM(IMMGraph& graph, const SeedSet& seeds, const ProgramArgs& args) {
    auto res = IMMResult3{};
    res.labels[0] = "Upper bound";
    res.labels[1] = "Lower bound";
    res.labels[2] = "(Not used)";

    // Upper bound
    LOG_INFO("SA_IMM: Starts upper bound.");
    auto argsUB = args;
    // Ca+ > Cr- > Cr > Ca
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

GreedyResult greedy(IMMGraph& graph, const SeedSet& seeds, const ProgramArgs& args) {
    // Prepare parameters
    setNodeStateGain(args["lambda"].get<double>());
    setNodeStatePriority(args["priority"].get<NodePriorityProperty>().priority);

    auto T = args.u["greedy-test-times"];
    auto res = GreedyResult{};
    auto gainV = std::vector<double>(graph.nNodes());

    auto initGainV = [&]() {
        // Initializes gain of other nodes to 0
        rs::fill(gainV, 0.0);
        // Initializes gain of chosen nodes and seed nodes to -inf
        utils::ranges::concatForEach([&](std::size_t v) {
            gainV[v] = halfMin<double>;
        }, res.boostedNodes, seeds.Sa(), seeds.Sr());
    };

    auto timer = Timer{};
    auto logPerPercentage = args["log-per-percentage"].get<double>();

    assert(graph.nNodes() >= seeds.size());

    auto K = args["k"].get<std::size_t>();
    auto nCandidates = graph.nNodes() - seeds.size();
    auto nAttempts = (nCandidates >= K)
            ? nCandidates * K - K * (K - 1) / 2
            : (K + 1) * K / 2;
    auto attemptCount = std::size_t{0};

    // Early stop if no more nodes can be chosen
    for (auto i = std::size_t{0}; i != K && i != nCandidates; i++) {
        initGainV();
        for (std::size_t v = 0; v != graph.nNodes(); v++) {
            // Skip excluded nodes
            if (gainV[v] < 0) {
                continue;
            }
            // Sets S = S + {v} temporarily
            res.boostedNodes.push_back(v);
            gainV[v] += simulateBoosted(graph, seeds, res.boostedNodes, T).totalGain;
            // Restores S
            res.boostedNodes.pop_back();

            attemptCount += 1;
            double r0 = 100.0 * (double)(attemptCount - 1) / (double)nAttempts;
            double r1 = 100.0 * (double)(attemptCount)     / (double)nAttempts;
            if (std::floor(r0 / logPerPercentage) != std::floor(r1 / logPerPercentage)) {
                LOG_INFO(format("Greedy: {:.1f}% finished. "
                                "Total time elapsed = {:.3f} sec.",
                                r1 + 1e-6, timer.elapsed().count()));
            }
        }

        auto v = rs::max_element(gainV) - gainV.begin();
        res.boostedNodes.push_back(v);
        LOG_INFO(format("Added boosted node #{} = {} with gain = {:.3f}. "
                        "Time used = {:.3f} sec.",
                        i + 1, v, gainV[v], timer.elapsed().count()));
    }

    res.result = simulate(graph, seeds, res.boostedNodes, args.u["test-times"]);
    return res;
}

template <std::invocable<std::size_t, std::size_t> Func>
GreedyResult naiveSolutionFramework(IMMGraph& graph, const SeedSet& seeds, const ProgramArgs& args, Func&& func) {
    // Prepare parameters
    setNodeStateGain(args["lambda"].get<double>());
    setNodeStatePriority(args["priority"].get<NodePriorityProperty>().priority);

    auto indices = std::vector<std::size_t>(graph.nNodes());
    std::iota(indices.begin(), indices.end(), 0);
    rs::sort(indices, [&](std::size_t u, std::size_t v) {
        // If exactly one of them is a seed node, then u is not seed (in other words, v is seed) => u > v
        if (seeds.contains(u) != seeds.contains(v)) {
            return seeds.contains(v);
        }
        // If neither is seed node, compare by func, func(u, v) == true => u > v
        if (!seeds.contains(u)) {
            return func(u, v);
        }
        // If both are seeds, index(u) > index(v) => u > v
        // (simply for strong ordering, no influence to the algorithm)
        return u > v;
    });

    auto k = std::min<std::size_t>(args.u["k"], graph.nNodes() - seeds.size());
    auto res = GreedyResult{};
    res.boostedNodes.assign(indices.begin(), indices.begin() + k);
    res.result = simulate(graph, seeds, res.boostedNodes, args.u["test-times"]);

    return res;
}

GreedyResult maxDegree(IMMGraph& graph, const SeedSet& seeds, const ProgramArgs& args) {
    return naiveSolutionFramework(graph, seeds, args, [&](std::size_t u, std::size_t v) {
        return graph.inDegree(u) + graph.outDegree(u) > graph.inDegree(v) + graph.outDegree(v);
    });
}

GreedyResult pageRank(IMMGraph& graph, const SeedSet& seeds, const ProgramArgs& args) {
    auto pr = graph::pageRank(graph);
    return naiveSolutionFramework(graph, seeds, args, [&](std::size_t u, std::size_t v) {
        return pr[u] > pr[v];
    });
}