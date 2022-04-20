//
// Created by dawnseeker on 2022/4/20.
//

#ifndef DAWNSEEKER_SIMULATE_H
#define DAWNSEEKER_SIMULATE_H

#include "immbasic.h"
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
template <rs::range Range> requires(std::convertible_to<rs::range_value_t<Range>, std::size_t>)
SimResultItem simulateBoostedOnce(IMMGraph& graph, const SeedSet& seeds, Range&& boostedNodes) {
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
 * @brief Simulates message propagation with given boosted nodes.

 * Simulation repeats for T times and the average is taken as result.
 * See simulateBoostedOnce for details.
 *
 * @param graph The whole graph
 * @param seeds The seed set
 * @param boostedNodes The list of boosted nodes
 * @param simTimes T, How many times to simulate
 * @return Total gain, positive gain and negative gain in average
 */
template <rs::range Range> requires (std::convertible_to<rs::range_value_t<Range>, std::size_t>)
SimResultItem simulateBoosted(
        IMMGraph& graph,
        const SeedSet& seeds,
        Range&& boostedNodes,
        std::size_t simTimes) {
    auto res = SimResultItem{};
    for (std::size_t i = 0; i != simTimes; i++) {
        res += simulateBoostedOnce(graph, seeds, boostedNodes);
    }
    res /= simTimes;
    return res;
}

/*!
 * @brief Simulates message propagation with and without given boosted nodes.
 *
 * Simulation repeats for T times with boosted nodes and without boosted nodes respectively.
 * See simulateBoosted for details.
 *
 * @param graph The whole graph
 * @param seeds The seed set
 * @param boostedNodes The list of boosted nodes
 * @param simTimes T, How many times to simulate
 * @return Simulation result with boosted nodes, without boosted nodes, and their difference
 */
template <rs::range Range> requires (std::convertible_to<rs::range_value_t<Range>, std::size_t>)
SimResult simulate(
        IMMGraph& graph,
        const SeedSet& seeds,
        Range&& boostedNodes,
        std::size_t simTimes) {
    auto res = SimResult{};
    res.withBoosted = simulateBoosted(graph, seeds, boostedNodes, simTimes);
    res.withoutBoosted = simulateBoosted(graph, seeds, vs::empty<std::size_t>, simTimes);
    res.diff = res.withBoosted - res.withoutBoosted;
    return res;
}

#endif //DAWNSEEKER_SIMULATE_H
