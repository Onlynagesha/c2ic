//
// Created by Onlynagesha on 2022/5/6.
//

#ifndef DAWNSEEKER_GREEDYSELECT_H
#define DAWNSEEKER_GREEDYSELECT_H

#include <utility>
#include "immbasic.h"
#include "Logger.h"
#include "PRRGraph.h"
#include "utils/numeric.h"
#include "utils/random.h"
#include "utils/ranges.h"
#include "utils/misc.h"

/*!
 * @brief Collection of all PRR-sketches in PR-IMM algorithm, for monotonic & submodular cases only.
 *
 * Supports greedy selection of boosted nodes.
 */
struct PRRGraphCollection {
    struct Node {
        std::size_t index;
        NodeState   centerStateTo;
    };

    // Simplified PRR-sketch type
    // centerState: original state of the center node if no boosted nodes impose influence
    // items: nodes in the PRR-sketch
    //  for [v, centerStateTo] in items:
    //   centerStateTo = which state the center node will become if node v is set boosted
    struct SimplifiedPRRGraph {
        NodeState           centerState;
        std::vector<Node>   items;
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

    /*!
     * @brief Default construction. Initialization must be done later
     */
    PRRGraphCollection() = default;

    /*!
     * @brief Constructs with graph size |V| and the seed set. Equivalent to init(n, seeds).
     * @param n The graph size |V|
     * @param seeds The seed set object.
     */
    explicit PRRGraphCollection(std::size_t n, SeedSet seeds) :
            n(n), seeds(std::move(seeds)), contrib(n), totalGain(n, 0.0) {
    }

    // Initializes with |V| and the seed set

    /*!
     * @brief Initializes with graph size |V| and the seed set.
     * @param n The graph size |V|
     * @param seeds The seed set object.
     */
    void init(std::size_t _n, SeedSet _seeds) {
        this->n = _n;
        this->seeds = std::move(_seeds);
        contrib.clear();
        contrib.resize(_n);
        totalGain.resize(_n, 0.0);
    }

    /*!
     * @brief Adds a PRR-sketch.
     * @param G The prr-sketch graph object.
     */
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
                    .centerState = G.centerState,
                    .items = std::move(prrList)
                }
            );
        }
    }

    /*!
     * @brief Merges two PRR-sketch collections by appending the given one to this.
     * @param other The PRR-sketch collection to be appended.
     */
    void merge(const PRRGraphCollection& other) {
        // Let R1 = Size of this->prrGraph, R2 = Size of other.prrGraph
        // for each [i, centerStateTo] in each other.contrib[v],
        //  the PRR-sketch index should shift by R1, i.e. [i + R1, centerStateTo] added to this->contrib[v]
        auto offset = prrGraph.size();
        // Step 1: Moves all the PRR-sketch to this->prrGraph
        rs::move(other.prrGraph, std::back_inserter(prrGraph));
        // Step 2: Merges contribution of each node v, with PRR-sketch index shifted by R1
        for (std::size_t v = 0; v < n; v++) {
            for (auto [i, s]: other.contrib[v]) {
                contrib[v].push_back(Node{.index = offset + i, .centerStateTo = s});
            }
        }
        // Step 3: Sums up total gain of each node v
        for (std::size_t v = 0; v < n; v++) {
            totalGain[v] += other.totalGain[v];
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
    /*!
     * @brief Selects k boosted nodes with greedy algorithm and gets the total gain value.
     *
     * ** FOR MONOTONE & SUB-MODULAR CASES ONLY **
     *
     * The gain value returned is a sum of all the |R| PRR-sketches
     * and you may need to divide it by |R|.
     *
     * Selection results are written via the given output iterator (if provided).
     * You can provide iter = nullptr to skip this process.
     *
     * For sub-modular cases, it's ensured the local solution is no worse than
     * (1 - 1/e) x 100% (63.2% approximately) of the global optimal in this greedy selection sub-problem.
     *
     * @param k How many boosted nodes to select
     * @param iter The output iterator to write the boosted node indices, or nullptr if not required.
     * @return The total gain value.
     */
    template <class OutIter>
    requires std::output_iterator<OutIter, std::size_t> || std::same_as<OutIter, std::nullptr_t>
    double select(std::size_t k, OutIter iter = nullptr) const {
        return _select(k, iter);
    }

    /*!
     * @brief Estimates the total memory usage in bytes.
     * @return Estimated total bytes used.
     */
    [[nodiscard]] std::size_t totalBytesUsed() const {
        // n and seeds
        auto bytes = sizeof(n) + seeds.totalBytesUsed();
        // Total bytes of prrGraph[][]
        bytes += utils::totalBytesUsed(prrGraph);
        // Total bytes of contrib[][]
        bytes += utils::totalBytesUsed(contrib);
        // Total bytes of totalGain[]
        bytes += utils::totalBytesUsed(totalGain);

        return bytes;
    }

    /*!
     * @brief Gets the sum of the number of nodes in each PRR-sketch, i.e. sum |R_i| for i = 1 to |R|.
     * @return Total number of nodes.
     */
    [[nodiscard]] std::size_t nTotalNodes() const {
        auto res = std::size_t{0};
        for (const auto& inner: prrGraph) {
            res += inner.items.size();
        }
        return res;
    }

    /*!
     * @brief Dumps information of the PRRGraphCollection object.
     * @return a multiline string, without trailing new-line character.
     */
    [[nodiscard]] std::string dump() const {
        auto info = "Graph size |V| = " + toString(n) + '\n';
        info += "Number of PRR-sketches stored = " + toString(prrGraph.size()) + '\n';

        // Dumps total and average number of nodes
        auto nNodes = nTotalNodes();
        info += "Total number of nodes = " + toString(nNodes)
                + ", " + toString(1.0 * (double) nNodes / (double) prrGraph.size(), 'f', 3)
                + " per PRR-sketch in average\n";

        // Dumps memory usage
        info += "Memory used = " + totalBytesUsedToString(totalBytesUsed());

        return info;
    }
};

/*!
 * @brief Collection of all PRR-sketches in SA-IMM and SA-RG-IMM algorithm. For non-monotonic cases.
 *
 * Greedy selection and random greedy selection are supported.
 */
struct PRRGraphCollectionSA {
    struct Node {
        std::size_t index;
        double      value;
    };

    // Number of nodes
    std::size_t n = 0;
    // Threshold of contribution.
    // For each center node v and boosted node s,
    //  if the average gain(s; v) (which is in the range [0, 1]) does not reach this threshold,
    //  it is filtered out (i.e. treated as 0) to same memory usage.
    double      threshold = 0.0;
    // Seed set
    SeedSet     seeds;
    // For each {v, g} in gainsByBoosted[s],
    //  node s makes total gain = g of all the samples to center node v, if the single node s is set as boosted
    std::vector<std::vector<Node>>  gainsByBoosted;
    // For each {s, g} in gainsToCenter[v],
    //  node s makes total gain = g of all the samples to center node v, i.e. gain(s; v) = g / countAsCenter[v]
    // For each v, gainsToCenter[v] is sorted by s
    std::vector<std::vector<Node>>  gainsToCenter;
    // countAsCenter[v] = how many times does node v appear as center in this collection
    std::vector<std::size_t>        countAsCenter;

    /*!
     * @brief Default constructor. Initialization later is required.
     */
    PRRGraphCollectionSA() = default;

    /*!
     * @brief Constructs with graph size |V|, gain threshold, and seed set.
     *
     * Equivalent to init(n, threshold, seeds).
     *
     * @param n The graph size |V|
     * @param threshold The gain threshold
     * @param seeds The seed set object
     */
    explicit PRRGraphCollectionSA(std::size_t _n, double _threshold, SeedSet _seeds) {
        init(_n, _threshold, std::move(_seeds));
    }

    /*!
     * @brief Initializes with graph size |V|, gain threshold, and seed set.
     * @param n The graph size |V|
     * @param threshold The gain threshold
     * @param seeds The seed set object
     */
    void init(std::size_t _n, double _threshold, SeedSet _seeds) {
        this->n = _n;
        this->threshold = _threshold;
        this->seeds = std::move(_seeds);

        gainsByBoosted.clear();
        gainsByBoosted.resize(n);

        gainsToCenter.clear();
        gainsToCenter.resize(n);

        countAsCenter.assign(n, 0);
    }

    /*!
     * @brief Adds all the gains to the given center v.
     *
     * For each node s, totalGainsByBoosted[s] = value in range [0, nSamples],
     * how much gain (sum of all the samples) does the boosted node s make to the given center v.
     *
     * @param center Index of the center node v
     * @param nSamples How many samples in this batch
     * @param totalGainsByBoosted A list of size |V|, see above for details.
     */
    void add(std::size_t center, std::size_t nSamples, const std::vector<double>& totalGainsByBoosted) {
        // Takes the existing part as sorted by boosted node index s
        auto sortedPart = rs::subrange{gainsToCenter[center]};

        for (std::size_t s = 0; s != n; s++) {
            if (totalGainsByBoosted[s] > 0) {
                // Attempts to find an existing record {s, g} in gainsToCenter[v] by binary searching by index s
                auto it = rs::lower_bound(sortedPart, s, rs::less{}, &Node::index);
                // If not found, appends a new record
                if (it == sortedPart.end()) {
                    gainsToCenter[center].push_back(Node{.index = s, .value = totalGainsByBoosted[s]});
                }
                // Otherwise, accumulates to the existing record
                else {
                    it->value += totalGainsByBoosted[s];
                }
            }
        }
        // Accumulates sample size of current center node
        countAsCenter[center] += nSamples;
        // Sorts gainsToCenter[v] by index s
        rs::sort(gainsToCenter[center], rs::less{}, &Node::index);
    }

private:
    enum class HowToChoose { GreedyOne, RandomK };

    // Initializes gainsByBoosted[][] from gainsToCenter[][]
    void _prepareGainsByBoosted() {
        // First clears the old
        gainsByBoosted.clear();
        gainsByBoosted.resize(n);

        for (std::size_t v = 0; v != n; v++) {
            for (auto [s, g]: gainsToCenter[v]) {
                // gain(s; v) = g / countAsCenter[v],
                // taking the average of all the samples with center node v
                auto gain = g / countAsCenter[v];
                // Filters out all the records under threshold
                if (gain >= threshold) {
                    gainsByBoosted[s].push_back(Node{.index = v, .value = gain});
                }
            }
        }
    }

#pragma clang diagnostic push
#pragma ide diagnostic ignored "ArgumentSelectionDefects"

    // Helper function of selection
    // Merges implementation of greedy selection and random greedy
    template <HowToChoose how, class OutIter>
    requires (std::output_iterator<OutIter, std::size_t> || std::same_as<OutIter, std::nullptr_t>)
    double _select(std::size_t k, OutIter iter = nullptr) {
        // First prepares gainsByBoosted[][]
        _prepareGainsByBoosted();

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
            // Seed nodes and previously selected nodes shall not be selected
            utils::ranges::concatForEach([&](std::size_t v) {
                totalGainsBy[v] = halfMin<double>;
            }, seeds.Sa(), seeds.Sr(), selected);

            // Current node chosen as a boosted node
            std::size_t cur;
            if constexpr (how == HowToChoose::GreedyOne) {
                // Greedy: chooses the boosted node with maximal total gain
                cur = rs::max_element(totalGainsBy) - totalGainsBy.begin();
            }
            else {
                // Random greedy: chooses the boosted node randomly among k candidates with maximal total gain
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

            LOG_DEBUG(format("Selected node #{0}: index = {1}, totalGainsBy[{1}] = {2:.3f}",
                             i, cur, totalGainsBy[cur]));

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

#pragma clang diagnostic pop

public:
    /*!
     * @brief Selects k boosted nodes with greedy algorithm.
     *
     * Selection results are written via the given output iterator (if provided).
     * You can provide iter = nullptr to skip this process.
     *
     * The total gain obtained is proved to have no less than (1 - 1/e) * OPT of this set-selection sub-problem.
     *
     * @param k How many boosted nodes to select
     * @param iter The output iterator to write the boosted node indices, or nullptr if not required.
     * @return The total gain value
     */
    template <class OutIter>
    requires (std::output_iterator<OutIter, std::size_t> || std::same_as<OutIter, std::nullptr_t>)
    double select(std::size_t k, OutIter iter = nullptr) {
        return _select<HowToChoose::GreedyOne>(k, iter);
    }

    /*!
     * @brief Selects k boosted nodes with random-greedy algorithm.
     *
     * Selection results are written via the given output iterator (if provided).
     * You can provide iter = nullptr to skip this process.
     *
     * The total gain obtained is proved to have no less than 1/e * OPT of this set-selection sub-problem.
     *
     * @param k How many boosted nodes to select
     * @param iter The output iterator to write the boosted node indices, or nullptr if not required.
     * @return The total gain value
     */
    template <class OutIter>
    requires (std::output_iterator<OutIter, std::size_t> || std::same_as<OutIter, std::nullptr_t>)
    double randomSelect(std::size_t k, OutIter iter = nullptr) {
        return _select<HowToChoose::RandomK>(k, iter);
    }

    /*!
     * @brief Estimates the total memory usage in bytes.
     * @return Estimated total bytes used.
     */
    [[nodiscard]] std::size_t totalBytesUsed() const {
        return sizeof(n)
        + sizeof(threshold)
        + seeds.totalBytesUsed()
        + utils::totalBytesUsed(gainsByBoosted)
        + utils::totalBytesUsed(gainsToCenter)
        + utils::totalBytesUsed(countAsCenter);
    }

    /*!
     * @brief Gets the sum of the number of records stored.
     *
     * Equivalent to sum_{0 <= v, s < |V|} I(gain(s; v) >= threshold).
     *
     * @return The Total number of records.
     */
    [[nodiscard]] std::size_t nTotalRecords() const {
        auto res = std::size_t{0};
        for (const auto& inner: gainsToCenter) {
            res += inner.size();
        }
        return res;
    }

    /*!
     * @brief Dumps information of the PRRGraphCollectionSA object.
     * @return a multiline string, without trailing new-line character.
     */
    [[nodiscard]] std::string dump() const {
        auto info = "Graph size |V| = " + toString(n) + '\n';

        // Dumps total and average number of nodes
        auto nRecords = nTotalRecords();
        info += "Total number of records = " + toString(nRecords)
                + ", " + toString(1.0 * (double) nRecords / (double) n, 'f', 3)
                + " per node in average\n";

        // Dumps memory used
        info += "Memory used = " + totalBytesUsedToString(totalBytesUsed());
        return info;
    }
};

#endif //DAWNSEEKER_GREEDYSELECT_H
