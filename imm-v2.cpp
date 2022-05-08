//
// Created by Onlynagesha on 2022/5/7.
//

#include <future>
#include <queue>
#include "graph/pagerank.h"
#include "greedyselect.h"
#include "imm.h"
#include "simulate.h"
#include "Timer.h"

struct GenerateSamplesResult {
    PRRGraphCollection  prrCollection;
    std::uint64_t       prrCount{};
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
void makeSketchFast(PRRGraphCollection& prrCollection,
                    IMMGraph&           graph,
                    PRRGraph&           prrGraph,
                    const SeedSet&      seeds,
                    std::size_t         center) {
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
void makeSketchesFast(PRRGraphCollection&   prrCollection,
                      IMMGraph&             graph,
                      const SeedSet&        seeds,
                      std::uint64_t         nSamples,
                      std::size_t           nThreads) {
    // nSamplesHere = Number of samples dispatched for this thread
    auto func = [](IMMGraph& graph, const SeedSet& seeds, std::uint64_t nSamplesHere) {
        auto collection = PRRGraphCollection(graph.nNodes(), seeds);
        auto prrGraph = PRRGraph{{
            {"nodes", graph.nNodes()},
            {"links", graph.nLinks()},
            {"maxIndex", graph.nNodes()}
        }};

        // Uniformly generates a center node in [0, n) each time
        static auto gen = createMT19937Generator();
        auto distCenter = std::uniform_int_distribution<std::size_t>(0, graph.nNodes() - 1);

        for (std::uint64_t i = 0; i < nSamplesHere; i++) {
            makeSketchFast(collection, graph, prrGraph, seeds, distCenter(gen));
        }
        return collection;
    };

    auto tasks = std::vector<std::future<PRRGraphCollection>>();
    for (std::size_t i = 0; i < nThreads; i++) {
        // Expected PRR-sketch index range = [first, last)
        auto first = nSamples * i       / nThreads;
        auto last  = nSamples * (i + 1) / nThreads;
        // Dispatches the task to generate (last - first) samples
        tasks.emplace_back(std::async(std::launch::async, func,
                                      std::ref(graph), std::cref(seeds), last - first));
    }

    // Merges all the results
    for (std::size_t i = 0; i < nThreads; i++) {
        prrCollection.merge(tasks[i].get());
    }
}

/*!
 * @brief Generates one PRR-sketch.
 *
 * This version has no constraints on monotonicity and submodularity, yet much slower.
 *
 * The result will be written to the prrGraph object.
 *
 * @param graph The whole graph
 * @param prrGraph The PRR-sketch object where the result is written
 * @param seeds The seed set
 * @param center The center node selected
 */
void makeSketchSlow(const IMMGraph& graph, PRRGraph& prrGraph, const SeedSet& seeds, std::size_t center) {
    // Gets a PRR-sketch with the specified center
    samplePRRSketch(graph, prrGraph, seeds, center);
    // Calculates each gain(v; prrGraph, center) for v in prrGraph
    // gain is implied as gain(v.centerStateTo) - gain(center.state)
    calculateCenterStateToSlow(prrGraph);
}

// Generate PRR-sketches
auto generateSamplesDynamic(IMMGraph& graph, const SeedSet& seeds, const DynamicArgs_PR_IMM& args)
{
    auto LB = double{ 1.0 };

    auto prrCollection = PRRGraphCollection(graph.nNodes(), seeds);
    // Count of PRR-sketches already generated
    auto prrCount = std::uint64_t{ 0 };

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

        auto nSamples = (std::uint64_t)std::min(theta, (double)args.sampleLimit) - prrCount;
        // Generates with multi-threading support
        makeSketchesFast(prrCollection, graph, seeds, nSamples, args.nThreads);
        prrCount += nSamples;

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
            LB = S * (double)graph.nNodes() / (1 + ns::sqrt2 * args.epsilon);
            break;
        }
    }

    if (prrCount < args.sampleLimit) {
        theta = 2.0 * graph.nNodes() * (double)std::pow(args.alpha + args.beta, 2.0)
                / LB / std::pow(args.epsilon, 2.0);
        LOG_INFO(format("LB = {:.0f}, theta = {:.0f}", LB, theta));
    }

    auto nSamples = (std::uint64_t)std::min(theta, (double)args.sampleLimit) - prrCount;
    makeSketchesFast(prrCollection, graph, seeds, nSamples, args.nThreads);

    return GenerateSamplesResult{
            .prrCollection = std::move(prrCollection),
            .prrCount = prrCount
    };
}

IMMResult PR_IMM_Dynamic(IMMGraph& graph, const SeedSet& seeds, const DynamicArgs_PR_IMM& args) {
    // Prepare parameters
    args.setEnv();

    auto resItem = IMMResultItem{};
    auto timer = Timer();
    auto [prrCollection, prrCount] = generateSamplesDynamic(graph, seeds, args);

    // totalGain = |V| * E(gains / |R|) where |V| = graph size, |R| = sample size
    resItem.totalGain = prrCollection.select(args.k, std::back_inserter(resItem.boostedNodes))
                        / (double)prrCount * graph.nNodes();
    resItem.timeUsed = timer.elapsed().count();
    resItem.memoryUsage = prrCollection.totalBytesUsed();

    LOG_INFO(format("PR_IMM: Finished generating PRR-sketches. Time used = {:.3f} sec.",
                    resItem.timeUsed));
    LOG_INFO(format("Dump PRR-sketch collection:\n{}", prrCollection.dump()));

    return IMMResult{
        .items = {{prrCount, std::move(resItem)}}
    };
}

IMMResult PR_IMM_Static(IMMGraph& graph, const SeedSet& seeds, const StaticArgs_PR_IMM& args) {
    // Prepare parameters
    args.setEnv();

    auto prrCollection = PRRGraphCollection(graph.nNodes(), seeds);
    auto res = IMMResult{};

    auto prrGraph = PRRGraph({
        {"nodes", graph.nNodes()},
        {"links", graph.nLinks()},
        {"maxIndex", graph.nNodes()}
    });

    auto timer = Timer{};
    for (std::uint64_t lastPrrCount = 0; std::uint64_t prrCount: args.nSamplesList) {
        // Appends until prrCount PRR-sketches
        makeSketchesFast(prrCollection, graph, seeds, prrCount - lastPrrCount, args.nThreads);

        auto resItem = IMMResultItem{};
        // totalGain = |V| * E(gains / |R|) where |V| = graph size, |R| = sample size
        resItem.totalGain = prrCollection.select(args.k, std::back_inserter(resItem.boostedNodes))
                            / (double)prrCount * graph.nNodes();
        resItem.timeUsed = timer.elapsed().count();
        resItem.memoryUsage = prrCollection.totalBytesUsed();

        LOG_INFO(format("Result item with {} PRR-sketches: {}",
                        prrCount, resItem));
        LOG_INFO(format("Dump PRR-sketch collection with {} samples: {}",
                        prrCount, prrCollection.dump()));

        // Adds a result record of current PRR-sketch count
        res.items[prrCount] = std::move(resItem);
        // Proceeds PRR-sketch count
        lastPrrCount = prrCount;
    }

    return res;
}

IMMResult PR_IMM(IMMGraph& graph, const SeedSet& seeds, const BasicArgs& args) {
    // Prepare global parameters
    args.setEnv();

    if (typeid(args) == typeid(DynamicArgs_PR_IMM)) {
        return PR_IMM_Dynamic(graph, seeds, dynamic_cast<const DynamicArgs_PR_IMM&>(args));
    } else if (typeid(args) == typeid(StaticArgs_PR_IMM)) {
        return PR_IMM_Static(graph, seeds, dynamic_cast<const StaticArgs_PR_IMM&>(args));
    } else {
        throw std::bad_cast();
    }
}

/*!
 * @brief Gets the center nodes after filtering by distance to seeds for SA-IMM-LB and SA-RG-IMM-LB algorithms.
 *
 * Only the nodes whose minimal distance from all the seed nodes <= distLimit are picked. Others are filtered out.
 * If distLimit = +inf, all the nodes are picked.
 *
 * @param graph The whole graph
 * @param seeds The seed set
 * @param distLimit Distance threshold from a center node to the seeds
 * @return A list of center nodes after filtering.
 */
auto getCenterList(IMMGraph& graph, const SeedSet& seeds, std::size_t distLimit) {
    auto res = std::vector<std::size_t>();

    // Simply picks all
    if (distLimit >= graph.nNodes()) {
        res.resize(graph.nNodes());
        std::iota(res.begin(), res.end(), 0);
        return res;
    }

    // BFS Initialization
    auto Q = std::queue<std::size_t>();
    for (auto& node: graph.nodes()) {
        node.dist = utils::halfMax<int>;
    }
    utils::ranges::concatForEach([&graph, &Q](const std::size_t& v) {
        Q.push(v);
        graph[v].dist = 0;
    }, seeds.Sa(), seeds.Sr());

    for (; !Q.empty(); Q.pop()) {
        auto cur = Q.front();

        for (auto& [to, link]: graph.fastLinksFrom(cur)) {
            if (to.dist == utils::halfMax<int>) {
                to.dist = graph[cur].dist + 1;

                if (to.dist <= distLimit) {
                    Q.push(index(to));
                    // Picks all the nodes with 1 <= distance <= distLimit
                    res.push_back(index(to));
                }
            }
        }
    }

    return res;
}

/*!
 * @brief Sub-process for SA-IMM-LB or SA-RG-IMM-LB algorithms.
 *
 * This process adds additional R samples for each candidate center nodes to prrCollection object.
 *
 * @param prrCollection The sample collection to which the results are written
 * @param centerCandidates List of candidate center nodes
 * @param nSamples R above, number of samples per center node
 * @param graph The whole graph
 * @param seeds The seed set
 * @param args Algorithm arguments
 */
void SA_IMM_LB_Static_Process(
        PRRGraphCollectionSA&           prrCollection,
        const std::vector<std::size_t>& centerCandidates,
        std::size_t                     nSamples,
        IMMGraph&                       graph,
        const SeedSet&                  seeds,
        const StaticArgs_SA_IMM_LB&     args) {
    auto finishCount = std::size_t{0};
    auto timer = Timer();

    auto update = [&](std::size_t v, const std::vector<double>& totalGainsByBoosted) {
        static auto mtx = std::mutex{};
        {
            auto lock = std::scoped_lock(mtx);
            prrCollection.add(v, nSamples, totalGainsByBoosted);

            double r0 = 100.0 * (double)finishCount       / (double)centerCandidates.size();
            double r1 = 100.0 * (double)(finishCount + 1) / (double)centerCandidates.size();
            if (std::floor(r0 / args.logPerPercentage) != std::floor(r1 / args.logPerPercentage)) {
                LOG_INFO(format("SA_IMM_LB: {:.1f}% finished. "
                                "Total gain records added = {}. "
                                "Total time elapsed since current process = {:.3f} sec.",
                                r1 + 1e-6, prrCollection.nTotalRecords(), timer.elapsed().count()));
            }

            finishCount += 1;
        }
    };

    auto func = [&](std::size_t firstIndex, std::size_t lastIndex) {
        auto prrGraph = PRRGraph({
            {"nodes", graph.nNodes()},
            {"links", graph.nLinks()},
            {"maxIndex", graph.nNodes()}
        });

        for (std::size_t i = firstIndex; i != lastIndex; i++) {
            auto v = centerCandidates[i];
            // curGainsByBoosted[s] = How much gain to current center node v if s is chosen as one boosted node
            auto curGainsByBoosted = std::vector<double>(graph.nNodes());

            for (std::size_t j = 0; j < nSamples; j++) {
                makeSketchSlow(graph, prrGraph, seeds, v);
                for (const auto& node: prrGraph.nodes()) {
                    double delta = gain(node.centerStateTo) - gain(prrGraph.centerState);
                    // Takes the sum
                    curGainsByBoosted[index(node)] += delta;
                }
            }
            update(v, curGainsByBoosted);
        }
    };

    auto tasks = std::vector<std::future<void>>();
    for (std::size_t i = 0; i < args.nThreads; i++) {
        auto first = centerCandidates.size() * i       / args.nThreads;
        auto last  = centerCandidates.size() * (i + 1) / args.nThreads;
        tasks.emplace_back(std::async(std::launch::async, func, first, last));
    }
    for (auto& t: tasks) {
        t.get();
    }
}

IMMResult SA_IMM_LB_Static(IMMGraph& graph, const SeedSet& seeds, const StaticArgs_SA_IMM_LB& args) {
    // Prepare global parameters
    args.setEnv();

    auto prrCollection = PRRGraphCollectionSA(graph.nNodes(), args.gainThreshold, seeds);
    auto usesRandomGreedy = args.algo == AlgorithmLabel::SA_RG_IMM;

    auto centerCandidates = getCenterList(graph, seeds, args.sampleDistLimit);
    LOG_INFO(format("#Candidates of center node: {} of {} ({:.2f}%)",
                    centerCandidates.size(), graph.nNodes(), 100.0 * centerCandidates.size() / graph.nNodes()));

    auto res = IMMResult{};
    auto timer = Timer{};

    for (auto lastNSamples = std::uint64_t{0}; auto nSamples: args.nSamplesList) {
        // Appends more samples with count = nSamples - lastNSamples
        SA_IMM_LB_Static_Process(prrCollection, centerCandidates, nSamples - lastNSamples, graph, seeds, args);

        auto resItem = IMMResultItem{};
        if (usesRandomGreedy) {
            LOG_INFO(format("SA-IMM: Performs greedy selection with nSamples = {}, k = {}",
                            nSamples, args.k));
            resItem.totalGain = prrCollection.randomSelect(args.k, std::back_inserter(resItem.boostedNodes));
        } else {
            LOG_INFO(format("SA-RG-IMM: Performs random greedy with nSamples = {}, k = {}",
                            nSamples, args.k));
            resItem.totalGain = prrCollection.select(args.k, std::back_inserter(resItem.boostedNodes));
        }

        resItem.timeUsed = timer.elapsed().count();
        resItem.memoryUsage = prrCollection.totalBytesUsed();
        LOG_INFO(format("Result item with {} samples per center node: {}",
                        nSamples, resItem));
        LOG_INFO(format("Dump sample collection with {} samples per center node: {}",
                        nSamples, prrCollection.dump()));

        res.items[nSamples] = resItem;
        lastNSamples = nSamples;
    }

    return res;
}

IMMResult SA_IMM_LB_Dynamic(IMMGraph& graph, const SeedSet& seeds, const DynamicArgs_SA_IMM_LB& args) {
    // for dynamic cases, the sample size is already calculated as theta
    return SA_IMM_LB_Static(
            graph, seeds, StaticArgs_SA_IMM_LB(args, ArgsSampleSizeStatic((std::uint64_t)args.theta)));
}

IMMResult SA_IMM_LB(IMMGraph& graph, const SeedSet& seeds, const BasicArgs& args) {
    // Prepare global parameters
    args.setEnv();

    if (typeid(args) == typeid(DynamicArgs_SA_IMM_LB)) {
        return SA_IMM_LB_Dynamic(graph, seeds, dynamic_cast<const DynamicArgs_SA_IMM_LB&>(args));
    } else if (typeid(args) == typeid(StaticArgs_SA_IMM_LB)) {
        return SA_IMM_LB_Static(graph, seeds, dynamic_cast<const StaticArgs_SA_IMM_LB&>(args));
    } else {
        throw std::bad_cast();
    }
}

IMMResult3 SA_IMM(IMMGraph& graph, const SeedSet& seeds, const BasicArgs& args_) {
    const auto& args = dynamic_cast<const Args_SA_IMM&>(args_);
    auto res = IMMResult3{};

    res.labels[0] = "Upper bound";
    auto argsUB = args.argsUB();
    LOG_INFO("SA-IMM: Arguments for upper bound: \n" + argsUB->dump());
    res[0] = PR_IMM(graph, seeds, *argsUB);

    res.labels[1] = "Lower bound";
    auto argsLB = args.argsLB();
    LOG_INFO("SA-IMM: Arguments for lower bound: \n" + argsLB->dump());
    res[1] = SA_IMM_LB(graph, seeds, *argsLB);

    return res;
}

GreedyResult greedy(IMMGraph& graph, const SeedSet& seeds, const BasicArgs& args_) {
    const auto& args = dynamic_cast<const GreedyArgs&>(args_);

    // Prepare parameters
    args.setEnv();

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
    assert(graph.nNodes() >= seeds.size());

    auto nCandidates = graph.nNodes() - seeds.size();
    auto nAttempts = (nCandidates >= args.k)
                     ? nCandidates * args.k - args.k * (args.k - 1) / 2
                     : (args.k + 1) * args.k / 2;
    auto attemptCount = std::size_t{0};

    // Early stop if no more nodes can be chosen
    for (auto i = std::size_t{0}; i != args.k && i != nCandidates; i++) {
        initGainV();
        for (std::size_t v = 0; v != graph.nNodes(); v++) {
            // Skip excluded nodes
            if (gainV[v] < 0) {
                continue;
            }
            // Sets S = S + {v} temporarily
            res.boostedNodes.push_back(v);
            gainV[v] += simulateBoosted(graph, seeds, res.boostedNodes, args.greedyTestTimes).totalGain;
            // Restores S
            res.boostedNodes.pop_back();

            attemptCount += 1;
            double r0 = 100.0 * (double)(attemptCount - 1) / (double)nAttempts;
            double r1 = 100.0 * (double)(attemptCount)     / (double)nAttempts;
            if (std::floor(r0 / args.logPerPercentage) != std::floor(r1 / args.logPerPercentage)) {
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

    return res;
}

template <std::invocable<std::size_t, std::size_t> Func>
GreedyResult naiveSolutionFramework(IMMGraph& graph, const SeedSet& seeds, const BasicArgs& args, Func&& func) {
    // Prepare parameters
    args.setEnv();

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

    auto k = std::min(args.k, graph.nNodes() - seeds.size());
    auto res = GreedyResult{};
    res.boostedNodes.assign(indices.begin(), indices.begin() + (ptrdiff_t)k);

    return res;
}

GreedyResult maxDegree(IMMGraph& graph, const SeedSet& seeds, const BasicArgs& args) {
    return naiveSolutionFramework(graph, seeds, args, [&](std::size_t u, std::size_t v) {
        return graph.inDegree(u) + graph.outDegree(u) > graph.inDegree(v) + graph.outDegree(v);
    });
}

GreedyResult pageRank(IMMGraph& graph, const SeedSet& seeds, const BasicArgs& args) {
    auto pr = graph::pageRank(graph);
    return naiveSolutionFramework(graph, seeds, args, [&](std::size_t u, std::size_t v) {
        return pr[u] > pr[v];
    });
}