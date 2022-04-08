#ifndef DAWNSEEKER_IMM_H
#define DAWNSEEKER_IMM_H

#include "args-v2.h"
#include "global.h"
#include "immbasic.h"
#include "Logger.h"
#include "PRRGraph.h"
#include "utils.h"
#include <cmath>
#include <locale>
#include <numbers>
#include <numeric>
#include <vector>

/*
* Collection of all PRR-sketches
*/
struct PRRGraphCollection {
    // Tag indicating the collection type shall be initialized later
    struct InitLater {};
    static inline auto initLater = InitLater{};

    struct Node {
        std::size_t index;
        NodeState centerStateTo;
    };

    // Simplified PRR-sketch type
    // centerState: original state of the center node if no boosted nodes impose influence
    // items: nodes in the PRR-sketch
    //  for [v, centerStateTo] in items:
    //   centerStateTo = which state the center node will become if node v is set boosted
    struct SimplifiedPRRGraph {
        NodeState centerState;
        std::vector<Node> items;
    };

    // Number of nodes in the graph, i.e. |V|
    std::size_t n{};
    // Seed set
    SeedSet seeds;
    // prrGraph[i] = the i-th PRR-sketch
    std::vector<SimplifiedPRRGraph> prrGraph;
    // contrib[v] = All the PRR-sketches where boosted node v changes the center node's state
    // for [i, centerStateTo] in contrib[v]:
    //  centerStateTo = which state the center node in the i-th PRR-sketch will become
    //                  if node v is set boosted
    std::vector<std::vector<Node>> contrib;
    // totalGain[v] = total gain of the node v
    std::vector<double> totalGain;

    // Default constructor is deleted to force initialization
    PRRGraphCollection() = delete;

    // Delays initialization with the tag type
    explicit PRRGraphCollection(InitLater) {
    }

    // Initializes with |V| and the seed set
    explicit PRRGraphCollection(std::size_t n, SeedSet seeds) :
            n(n), seeds(std::move(seeds)), contrib(n), totalGain(n, 0.0) {
    }

    // Initializes with |V| and the seed set
    void init(std::size_t _n, SeedSet _seeds) {
        this->n = _n;
        this->seeds = std::move(_seeds);
        contrib.clear();
        contrib.resize(_n);
        totalGain.resize(_n, 0.0);
    }

    // Adds a PRR-sketch
    void add(const PRRGraph& G) {
        auto prrList = std::vector<Node>();
        // Index of the PRR-sketch to be added
        auto prrListId = prrGraph.size();

        for (const auto& node: G.nodes()) {
            double nodeGain = gain(node.centerStateTo) - gain(G.centerState);
            // Zero-gain nodes are skipped to save memory usage
            if (nodeGain <= 0.0) {
                continue;
            }
            auto v = node.index();
            // Boosted node v, and the state changes the center node will change to
            prrList.push_back(Node{.index = v, .centerStateTo = node.centerStateTo});
            // Boosted node v has influence on current PRR-sketch
            contrib[v].push_back(Node{.index = prrListId, .centerStateTo = node.centerStateTo});
            totalGain[v] += nodeGain;
        }

        // Empty PRR-sketch (if no node makes positive gain) is also skipped to same memory usage
        if (!prrList.empty()) {
            prrGraph.push_back(
                    SimplifiedPRRGraph{
                            .centerState = G.centerState, .items = std::move(prrList)
                    }
            );
        }
    }

private:

    // Helper non-const function of greedy selection
    template <class OutIter>
    requires std::output_iterator<OutIter, std::size_t> || std::same_as<OutIter, std::nullptr_t>
    double _select(std::size_t k, OutIter iter) const {
        double res = 0.0;
        // Makes a copy of the totalGain[] to update values during selection
        auto totalGainCopy = totalGain;
        // Makes a copy of all the center states
        // centerStateCopy[i] = center state of the i-th PRR-sketch
        //  after modification during greedy selection
        auto centerStateCopy = std::vector<NodeState>(prrGraph.size());
        rs::transform(prrGraph, centerStateCopy.begin(), &SimplifiedPRRGraph::centerState);
        // The seeds shall not be selected
        for (auto a: seeds.Sa()) {
            totalGainCopy[a] = halfMin<double>;
        }
        for (auto r: seeds.Sr()) {
            totalGainCopy[r] = halfMin<double>;
        }

        for (std::size_t i = 0; i < k; i++) {
            // v = The node chosen in this turn, with the highest total gain
            std::size_t v = rs::max_element(totalGainCopy) - totalGainCopy.begin();
            // Output is only enabled when iter != nullptr
            if constexpr (!std::is_same_v<OutIter, std::nullptr_t>) {
                *iter++ = v;
            }
            res += totalGainCopy[v];
            LOG_DEBUG(format("Selected node #{}: index = {}, result += {:.2f}", i + 1, v, totalGainCopy[v]));
            // Sets the total gain of selected node as -inf
            //  so that it can never be selected again
            totalGainCopy[v] = halfMin<double>;

            // Impose influence to all the PRR-sketches by node v
            for (auto[prrId, centerStateTo]: contrib[v]) {
                // Attempts to update the center state of current PRR-sketch...
                auto cmp = centerStateTo <=> centerStateCopy[prrId];
                // ...only if the update makes higher priority.
                // (otherwise, the PRR-sketch may have been updated by other selected boosted nodes)
                if (cmp != std::strong_ordering::greater) {
                    continue;
                }
                double curGain = gain(centerStateTo) - gain(centerStateCopy[prrId]);
                // After the center state of the prrId-th PRR-sketch changed from C0 to C1,
                //  all other nodes that may change the same PRR-sketch will make lower gain,
                //  from (C2 - C0) to (C2 - C1), diff = C1 - C0
                for (const auto&[j, s]: prrGraph[prrId].items) {
                    totalGainCopy[j] -= curGain;
                }
                // Updates state of the prrId-th PRR-sketch
                centerStateCopy[prrId] = centerStateTo;
            }
        }

        return res;
    }

public:
    // Selects k boosted nodes with greedy algorithm
    // Returns the total gain contributed by the selected nodes. 
    // Note that the gain value returned is a sum of all the |R| PRR-sketches
    //  and you may need to divide it by |R|.
    // Selection results are written via an output iterator.
    // You can provide iter = nullptr to skip this process.
    // For sub-modular cases, it's ensured the local solution is no worse than
    //  (1 - 1/e) x 100% (63.2% approximately) of the global optimal.
    // ** FOR MONOTONE & SUB-MODULAR CASES ONLY **
    template <class OutIter>
    requires std::output_iterator<OutIter, std::size_t> || std::same_as<OutIter, std::nullptr_t>
    double select(std::size_t k, OutIter iter = nullptr) const {
        return _select(k, iter);
    }

#pragma clang diagnostic push
#pragma ide diagnostic ignored "bugprone-sizeof-container"

    /*
    * Returns an estimation of total bytes used
    */
    [[nodiscard]] std::size_t totalBytesUsed() const {
        // n and seeds
        auto bytes = sizeof(n) + seeds.totalBytesUsed();
        // Total bytes of prrGraph[][]
        bytes += ::totalBytesUsed(prrGraph);
        // Total bytes of contrib[][]
        bytes += ::totalBytesUsed(contrib);
        // Total bytes of totalGain[]
        bytes += ::totalBytesUsed(totalGain);

        return bytes;
    }

#pragma clang diagnostic pop

    /*
    * Returns the sum of the number of nodes in each PRR-sketch
    * i.e. sum |R_i| for i = 1 to |R|
    */
    [[nodiscard]] std::size_t nTotalNodes() const {
        auto res = std::size_t{0};
        for (const auto& inner: prrGraph) {
            res += inner.items.size();
        }
        return res;
    }

    /*
    * Dumps info on the PRRGraphCollection object
    * Returns a string of multiple lines, an '\n' appended at the tail
    */
    [[nodiscard]] std::string dump() const {
        // The default locale
        auto loc = std::locale("");
        auto info = format("Graph size |V| = {}\nNumber of PRR-sketches stored = {}\n", n, prrGraph.size());

        // Dumps total number of nodes 
        auto nNodes = nTotalNodes();
        info += format(
                loc, "Total number of nodes = {}, {:.3f} per PRR-sketch in average\n", nNodes,
                1.0 * (double) nNodes / (double) prrGraph.size());

        // Dumps memory used
        info += format(loc, "Memory used = {}", totalBytesUsedToString(totalBytesUsed()));
        return info;
    }
};

/*
* Collection of all PRR-sketches for SA-IMM algorithm
*/
struct PRRGraphCollectionSA {
    struct InitLater {};
    static constexpr auto initLater = InitLater{};

    struct Node {
        std::size_t index;
        double value;
    };

    // Number of nodes
    std::size_t n{};
    // Threshold of contribution.
    // For each center node v and boosted node s, if gain(s; v) does not reach this threshold,
    //  it is filtered out to same memory usage.
    double threshold{};
    // Seed set
    SeedSet seeds;
    // For each {v, g} in gainsByBoosted[s],
    //  node s makes gain = g to center node v, if the single node s is set as boosted
    std::vector<std::vector<Node>> gainsByBoosted;

    // Default ctor. is deleted to force initialization
    PRRGraphCollectionSA() = delete;

    // Explicitly marks to initialize later
    explicit PRRGraphCollectionSA(InitLater) {
    }

    // Initialize with n = graph size |V|, threshold, and seed set
    explicit PRRGraphCollectionSA(std::size_t _n, double _threshold, SeedSet _seeds) :
            n(_n), threshold(_threshold), seeds(std::move(_seeds)), gainsByBoosted(_n) {
    }

    // Initialize with n = graph size |V|, threshold, and seed set
    void init(std::size_t _n, double _threshold, SeedSet _seeds) {
        this->n = _n;
        this->threshold = _threshold;
        this->seeds = std::move(_seeds);

        gainsByBoosted.clear();
        gainsByBoosted.resize(n);
    }

    // Adds all the gains to current center
    // curGainsByBoosted[s] = a value in [0, 1], how much gain (in average of all the samples)
    //  does the boosted node s make to current center
    void add(std::size_t center, const std::vector<double>& curGainsByBoosted) {
        for (std::size_t s = 0; s != n; s++) {
            // Nodes without or with too little contribution are skipped to save memory usage
            if (curGainsByBoosted[s] > threshold) {
                gainsByBoosted[s].push_back(Node{.index = center, .value = curGainsByBoosted[s]});
//                gainsToCenter[center].push_back(Node{.index = s, .value = curGainsByBoosted[s]});
            }
        }
    }

private:
    enum class HowToChoose { GreedyOne, RandomK };

#pragma clang diagnostic push
#pragma ide diagnostic ignored "ArgumentSelectionDefects"

    // Helper function of selection
    // Merges greedy selection and random greedy together
    template <HowToChoose how, class OutIter>
    requires (std::output_iterator<OutIter, std::size_t> || std::same_as<OutIter, std::nullptr_t>)
    double _select(std::size_t k, OutIter iter = nullptr) {
        double res = 0.0;
        auto selected = std::vector<std::size_t>();
        selected.reserve(k);

        // totalGainsBy[s] = sum of extra gain by one extra boosted node s
        auto totalGainsBy = std::vector<double>(n);
        // maxGainTo[v] = maximum gain to center node v among all the selected boosted nodes
        auto maxGainTo = std::vector<double>(n);

        // Resets totalGainsBy[s] for each s according to maxGainTo[v] for each center v
        //  with contribution of selected boosted nodes
        for (std::size_t i = 0; i < k; i++) {
            rs::fill(totalGainsBy, 0.0);
            for (std::size_t s = 0; s < n; s++) {
                for (const auto&[v, g]: gainsByBoosted[s]) {
                    totalGainsBy[s] += std::max(0.0, g - maxGainTo[v]);
                }
            }
            // Seed nodes shall not be selected
            for (auto a: seeds.Sa()) {
                totalGainsBy[a] = halfMin<double>;
            }
            for (auto r: seeds.Sr()) {
                totalGainsBy[r] = halfMin<double>;
            }
            // Selected nodes shall not be selected again
            for (auto s: selected) {
                totalGainsBy[s] = halfMin<double>;
            }

            // Current node chosen as a boosted node
            std::size_t cur;
            if constexpr (how == HowToChoose::GreedyOne) {
                cur = rs::max_element(totalGainsBy) - totalGainsBy.begin();
            }
            else {
                auto nCandidates = std::min(k, n - selected.size() - seeds.size());
                auto indices = std::vector<std::size_t>(n);
                std::iota(indices.begin(), indices.end(), std::size_t{0});
                // Gets the first k nodes that makes the maximum total gain
                rs::nth_element(
                        indices, indices.begin() + (std::ptrdiff_t) nCandidates, [&](std::size_t s1, std::size_t s2) {
                            return totalGainsBy[s1] > totalGainsBy[s2];
                        }
                );
                // Picks one of the k candidates uniformly randomly
                static auto gen = createMT19937Generator();
                auto dist = std::uniform_int_distribution<std::size_t>(0, nCandidates - 1);
                cur = indices[dist(gen)];
            }

            res += totalGainsBy[cur];
            selected.push_back(cur);
            if constexpr (!std::same_as<OutIter, std::nullptr_t>) {
                *iter++ = cur;
            }
            // Updates all the max gain of each center node v
            for (const auto&[v, g]: gainsByBoosted[cur]) {
                maxGainTo[v] = std::max(maxGainTo[v], g);
            }
        }

        return res;
    }

public:
    // Selects k boosted nodes with greedy algorithm
    //  that is proved to have no less than (1 - 1/e) * OPT of this set-selection sub-problem
    // Returns the total gain contributed by the selected nodes.
    // Selection results are written via an output iterator.
    // You can provide iter = nullptr to skip this process.
    template <class OutIter>
    requires (std::output_iterator<OutIter, std::size_t> || std::same_as<OutIter, std::nullptr_t>)
    double select(std::size_t k, OutIter iter = nullptr) {
        return _select<HowToChoose::GreedyOne>(k, iter);
    }

    // Selects k boosted nodes with random-greedy algorithm
    //  that is proved to have no less than 1/e * OPT of this set-selection sub-problem
    // Returns the total gain contributed by the selected nodes.
    // Selection results are written via an output iterator.
    // You can provide iter = nullptr to skip this process.
    template <class OutIter>
    requires (std::output_iterator<OutIter, std::size_t> || std::same_as<OutIter, std::nullptr_t>)
    double randomSelect(std::size_t k, OutIter iter = nullptr) {
        return _select<HowToChoose::RandomK>(k, iter);
    }

#pragma clang diagnostic pop

    [[nodiscard]] std::size_t totalBytesUsed() const {
        return sizeof(n) + sizeof(threshold) + seeds.totalBytesUsed() + ::totalBytesUsed(gainsByBoosted);
    }

    [[nodiscard]] std::size_t nTotalRecords() const {
        auto res = std::size_t{0};
        for (const auto& inner: gainsByBoosted) {
            res += inner.size();
        }
        return res;
    }

    [[nodiscard]] std::string dump() const {
        // The default locale
        auto loc = std::locale("");
        auto info = format("Graph size |V| = {}\n", n);

        // Dumps total number of nodes
        auto nRecords = nTotalRecords();
        info += format(
                loc, "Total number of records = {}, {:.3f} per node in average\n", nRecords,
                1.0 * (double) nRecords / (double) n
        );

        // Dumps memory used
        info += format(loc, "Memory used = {}", totalBytesUsedToString(totalBytesUsed()));
        return info;
    }
};

struct IMMResult {
    std::vector<std::size_t> boostedNodes;
    double totalGain;
};

inline std::string toString(const IMMResult& item, int indent = -1, int indentOutside = 0) {
    auto res = std::string("{");
    if (indent >= 0) {
        res += "\n" + std::string(indent, ' ');
    }
    res += ".totalGain = " + (item.totalGain <= halfMin<double> ? "-inf" : toString(item.totalGain, 'f', 3)) + ", ";
    if (indent >= 0) {
        res += "\n" + std::string(indent, ' ');
    }
    res += ".boostedNodes = " + join(item.boostedNodes, ", ", "{", "}");
    if (indent >= 0) {
        res += "\n";
    }
    res += std::string(indentOutside, ' ') + '}';
    return res;
}

struct IMMResult3 : std::array<IMMResult, 3> {
    std::array<std::string, 3> labels;
    std::size_t bestIndex;

    [[nodiscard]] const IMMResult& best() const {
        return operator [](bestIndex);
    }
};

inline std::string toString(const IMMResult3& item, int indent = 4, int indentInside = -1) {
    if (indent >= 0 && indentInside >= 0) {
        indentInside += indent;
    }
    auto res = std::string("{");
    for (std::size_t i = 0; i < 3; i++) {
        if (indent >= 0) {
            res += "\n" + std::string(indent, ' ');
        }
        res += format("'{0}': {1}, ", item.labels[i], toString(item[i], indentInside, std::max(0, indent)));
    }
    if (indent >= 0) {
        res += "\n" + std::string(indent, ' ');
    }
    res += format("best: '{}'", item.labels[item.bestIndex]);

    if (indent >= 0) {
        res += '\n';
    }
    res += '}';
    return res;
}

/*
* Returns the result of PR_IMM algorithm with given seed set and arguments
* ** FOR MONOTONIC & SUB-MODULAR CASES **
*/
IMMResult PR_IMM(IMMGraph& graph, const SeedSet& seeds, const AlgorithmArgs& args);

/*
 * Returns the result of SA_IMM algorithm (for monotonic cases)
 *  or SA_RG_IMM algorithm (for non-monotonic cases) with given seed set and arguments
 */
IMMResult3 SA_IMM(IMMGraph& graph, const SeedSet& seeds, const AlgorithmArgs& args);

struct GreedyResult {
    std::vector<std::size_t> boostedNodes;
    SimResult result;
};

/*!
 * @brief Solves with greedy algorithm.
 *
 * Performs greedy algorithm as the following pseudocode:
 *
 *      //input:
 *      //  G(V, E) as the graph
 *      //  k as the budget
 *      //  S_a and S_r as seed sets of positive and negative message resp.
 *      //  T as the number of repeat times during each simulation, given as args["greedy-test-times"]
 *      //  Ts as the number of repeat times during the final simulation, given as args["test-times"]
 *      //output:
 *      //  S as the selected boosted node set such that |S| <= k
 *      S = {}
 *      for i = 1 to k:
 *          for v in V - S_a - S_r - S
 *              // Simulate T times and take the average
 *              gain(v) = simulate(G, S + {v}, T).totalGain
 *          S += { argmax_v gain(v) }
 *      return S, simulate(G, S, Ts)
 *
 * @param graph The whole graph
 * @param seeds The seed set
 * @param args Arguments of the algorithm
 * @return an object that contains boosted node set, and the final simulation result
 */
GreedyResult greedy(IMMGraph& graph, const SeedSet& seeds, const AlgorithmArgs& args);

/*!
 * @brief Solves with max-degree algorithm.
 *
 * Simply picks the nodes with maximum degree.
 *
 * Simulation is performed after picking boosted nodes, with repeat times given as args["test-times"]
 *
 * @param graph The whole graph
 * @param seeds The seed set
 * @param args Arguments of the algorithm
 * @return an object that contains boosted node set, and the final simulation result
 */
GreedyResult maxDegree(IMMGraph& graph, const SeedSet& seeds, const AlgorithmArgs& args);

/*!
 * @brief Solves with PageRank algorithm.
 *
 * Simply picks the nodes with maximum PageRanks.
 *
 * Simulation is performed after picking boosted nodes, with repeat times given as args["test-times"]
 *
 * @param graph The whole graph
 * @param seeds The seed set
 * @param args Arguments of the algorithm
 * @return an object that contains boosted node set, and the final simulation result
 */
GreedyResult pageRank(IMMGraph& graph, const SeedSet& seeds, const AlgorithmArgs& args);

#endif //DAWNSEEKER_IMM_H