//
// Created by dawnseeker on 2022/3/27.
//

#include "immbasic.h"
#include <queue>

/*
 * Simulates message propagation with given boosted nodes
 * Returns the total gain of all the nodes, Ca and Ca+ counted as lambda, Cr as lambda - 1, Cr- as 0
 */
SimResultItem simulate(IMMGraph& graph, const SeedSet& seeds, const std::vector<std::size_t>& boostedNodes) {
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

SimResultItem simulateBoosted(
        IMMGraph& graph,
        const SeedSet& seeds,
        const std::vector<std::size_t>& boostedNodes,
        std::size_t simTimes)
{
    auto res = SimResultItem{};
    for (std::size_t i = 0; i != simTimes; i++) {
        res += simulate(graph, seeds, boostedNodes);
    }
    res /= simTimes;
    return res;
}

/*
 * Simulates message propagation with given boosted nodes
 * Each simulation sums up the gain of all the nodes,
 *  Ca and Ca+ counted as lambda, Cr as lambda - 1, Cr- and None as 0.
 * Returns:
 *  (1) Results with boosted nodes
 *  (2) Results without boosted nodes
 *  (3) Difference of the two, with - without
 */
SimResult simulate(IMMGraph& graph, const SeedSet& seeds,
                   const std::vector<std::size_t>& boostedNodes, std::size_t simTimes) {
    static auto emptyList = std::vector<std::size_t>();
    auto res = SimResult{};
    res.withBoosted = simulateBoosted(graph, seeds, boostedNodes, simTimes);
    res.withoutBoosted = simulateBoosted(graph, seeds, emptyList, simTimes);
    res.diff = res.withBoosted - res.withoutBoosted;
    return res;
}