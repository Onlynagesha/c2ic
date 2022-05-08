//
// Created by dawnseeker on 2022/4/20.
//

#ifndef DAWNSEEKER_SIMULATE_H
#define DAWNSEEKER_SIMULATE_H

#include "immbasic.h"
#include <future>
#include <queue>

struct SimResultItem {
    double positiveGain;    // positive = sum of all the gain(v) > 0
    double negativeGain;    // negative = sum of all the gain(v) < 0
    double totalGain;       // total = positive + negative

    void add(double value) {
        totalGain += value;
        if (value > 0.0) {
            positiveGain += value;
        } else {
            negativeGain += value;
        }
    }

    SimResultItem& operator += (const SimResultItem& rhs) {
        positiveGain += rhs.positiveGain;
        negativeGain += rhs.negativeGain;
        totalGain += rhs.totalGain;
        return *this;
    }

    SimResultItem& operator /= (std::size_t n) {
        positiveGain /= (double) n;
        negativeGain /= (double) n;
        totalGain /= (double) n;
        return *this;
    }

    SimResultItem operator - (const SimResultItem& rhs) const {
        return SimResultItem{
                .positiveGain = this->positiveGain - rhs.positiveGain,
                .negativeGain = this->negativeGain - rhs.negativeGain,
                .totalGain = this->totalGain - rhs.totalGain
        };
    }
};

inline std::string toString(const SimResultItem& item) noexcept {
    return format(
            "{{.totalGain = {:.2f}, .positiveGain = {:.2f}, .negativeGain = {:.2f}}}",
            item.totalGain, item.positiveGain, item.negativeGain);
}

struct SimResult {
    SimResultItem withBoosted;
    SimResultItem withoutBoosted;
    SimResultItem diff;     // diff = without boosted - without boosted
};

// Converts to a multi-line string
inline std::string toString(const SimResult& res) noexcept {
    return format(
            "{{\n"
            "       .withBoosted = {0}\n"
            "    .withoutBoosted = {1}\n"
            "              .diff = {2}\n}}",
            res.withBoosted, res.withoutBoosted, res.diff);
}

/*
 * Simulates message propagation with given boosted nodes
 * Returns the total gain of all the nodes, Ca and Ca+ counted as lambda, Cr as lambda - 1, Cr- as 0
 */

/*!
 * Simulates message propagation with given boosted nodes.
 *
 * Each simulation sums up the gain of all the nodes,
 * Ca and Ca+ counted as lambda, Cr as lambda - 1, Cr- and None as 0.
 *
 * @param graph The whole graph
 * @param seeds The seed set
 * @param boostedNodes The list of boosted nodes
 * @return Total gain, positive gain and negative gain in average
 */
template <rs::range Range>
SimResultItem simulateBoostedOnce(IMMGraph& graph, const SeedSet& seeds, Range&& boostedNodes)
requires(std::convertible_to<rs::range_value_t<Range>, std::size_t>) {
    // Initializes all the link states
    IMMLink::refreshAllStates();
    // Initializes all the nodes: state = None, dist = +inf, boosted = false
    for (auto& node: graph.nodes()) {
        node.state = NodeState::None;
        node.dist = halfMax<int>;
        node.boosted = false;
    }
    // Marks all the boosted nodes
    for (auto s: boostedNodes) {
        graph[s].boosted = true;
    }

    auto Q = std::queue<IMMNode*>();
    // Adds all the seeds to queue first
    for (auto a: seeds.Sa()) {
        graph[a].state = NodeState::Ca;
        graph[a].dist = 0;
        Q.emplace(graph.fastNode(a));
    }
    for (auto r: seeds.Sr()) {
        graph[r].state = NodeState::Cr;
        graph[r].dist = 0;
        Q.emplace(graph.fastNode(r));
    }

    for (; !Q.empty(); Q.pop()) {
        auto& cur = *Q.front();
        // Boosted nodes may change its state,
        //  making positive message boosted and negative message neutralized
        if (cur.boosted) {
            if (cur.state == NodeState::Ca) {
                cur.state = NodeState::CaPlus;
            } else if (cur.state == NodeState::Cr) {
                cur.state = NodeState::CrMinus;
            }
        }
        for (const auto& [to, link]: graph.fastLinksFrom(cur)) {
            // Checks the link state
            // For Ca+ message, either boosted or active is OK.
            // For others, only active.
            if (link.getState() == LinkState::Blocked ||
                link.forceGetState() == LinkState::Boosted && cur.state != NodeState::CaPlus) {
                continue;
            }
            if (cur.dist + 1 < to.dist) {
                // If to is never visited before, adds it to queue
                if (to.dist == halfMax<int>) {
                    Q.emplace(&to);
                }
                // Updates dist and state, message propagates along cur -> to
                to.state = cur.state;
                to.dist = cur.dist + 1;
            }
                // Some other message has arrived in the same round, but current one has higher priority
            else if (cur.dist + 1 == to.dist && compare(cur.state, to.state) > 0) {
                to.state = cur.state;
            }
        }
    }

    auto res = SimResultItem{};
    for (const auto& node: graph.nodes()) {
        res.add(gain(node.state));
    }
    return res;
}

/*!
 * @brief Simulates message propagation with given boosted nodes, with multi-threading support.

 * Simulation repeats for T times and the average is taken as result.
 * See simulateBoostedOnce for details.
 *
 * @param graph The whole graph
 * @param seeds The seed set
 * @param boostedNodes The list of boosted nodes
 * @param simTimes T, How many times to simulate
 * @param nThreads How many threads used for simulation
 * @return Total gain, positive gain and negative gain in average
 */
template <rs::range Range>
SimResultItem simulateBoosted(
        IMMGraph&       graph,
        const SeedSet&  seeds,
        Range&&         boostedNodes,
        std::size_t     simTimes,
        std::size_t     nThreads = 1)
        requires (std::convertible_to<rs::range_value_t<Range>, std::size_t>) {
    // simTimesHere = Number of simulation times dispatched for current task worker
    auto func = [&](std::size_t simTimesHere) {
        // Copying the graph is necessary since threads shall not influence each other.
        auto graphCopy = graph;
        auto res = SimResultItem{};
        for (std::size_t i = 0; i != simTimesHere; i++) {
            res += simulateBoostedOnce(graphCopy, seeds, boostedNodes);
        }
        return res;
    };

    auto tasks = std::vector<std::future<SimResultItem>>{};
    for (std::size_t i = 0; i < nThreads; i++) {
        // Current task worker performs simulations with index in [first, last)
        auto first = simTimes * i / nThreads;
        auto last = simTimes * (i + 1) / nThreads;
        tasks.emplace_back(std::async(std::launch::async, func, last - first));
    }

    auto res = SimResultItem{};
    // Sums up
    for (std::size_t i = 0; i < nThreads; i++) {
        res += tasks[i].get();
    }
    // ... and then takes the average
    res /= simTimes;
    return res;
}

/*!
 * @brief Simulates message propagation with and without given boosted nodes, with multi-threading support.
 *
 * Simulation repeats for T times with boosted nodes and without boosted nodes respectively.
 * See simulateBoosted for details.
 *
 * @param graph The whole graph
 * @param seeds The seed set
 * @param boostedNodes The list of boosted nodes
 * @param simTimes T, How many times to simulate
 * @param nThreads How many threads used for simulation
 * @return Simulation result with boosted nodes, without boosted nodes, and their difference
 */
template <rs::range Range>
SimResult simulate(
        IMMGraph&       graph,
        const SeedSet&  seeds,
        Range&&         boostedNodes,
        std::size_t     simTimes,
        std::size_t     nThreads = 1)
        requires (std::convertible_to<rs::range_value_t<Range>, std::size_t>) {
    auto res = SimResult{};
    res.withBoosted = simulateBoosted(graph, seeds, boostedNodes, simTimes, nThreads);
    res.withoutBoosted = simulateBoosted(graph, seeds, vs::empty<std::size_t>, simTimes, nThreads);
    res.diff = res.withBoosted - res.withoutBoosted;
    return res;
}

/*!
 * @brief Simulates message propagation with and without given boosted nodes, with multi-threading support.
 *
 * Assumes that the boosted nodes are sorted by descending order of influence,
 * i.e. the selection result of K = K2 < K1 is simply the prefix of result with K = K1 of length K2.
 *
 * A list of boosted node count K's is provided. For each K,
 * only the first min{K, Total} boosted nodes are used for simulation.
 *
 * See simulate(graph, seeds, boostedNodes, simTimes, nThreads) for details.
 *
 * @param graph The whole graph
 * @param seeds The seed set
 * @param boostedNodes The list of boosted nodes
 * @param kList The list of K's
 * @param simTimes T, How many times to simulate
 * @param nThreads How many threads used for simulation
 * @return A list of simulation results for each K, with boosted nodes, without boosted nodes, and their difference
 */
template <rs::range NodeRange, rs::range KRange>
std::vector<SimResult> simulate(
        IMMGraph&       graph,
        const SeedSet&  seeds,
        NodeRange&&     boostedNodes,
        KRange&&        kList,
        std::size_t     simTimes,
        std::size_t     nThreads = 1)
        requires (std::convertible_to<rs::range_value_t<NodeRange>, std::size_t>)
        && (std::convertible_to<rs::range_value_t<KRange>, std::size_t>) {
    auto noBoosted = simulateBoosted(graph, seeds, vs::empty<std::size_t>, simTimes, nThreads);
    auto res = std::vector<SimResult>{};

    for (auto k: kList) {
        auto withBoosted = simulateBoosted(graph, seeds, boostedNodes | vs::take(k), simTimes, nThreads);
        res.push_back(SimResult{
            .withBoosted = withBoosted,
            .withoutBoosted = noBoosted,
            .diff = withBoosted - noBoosted
        });
    }

    return res;
}

#endif //DAWNSEEKER_SIMULATE_H
