//
// Created by dawnseeker on 2022/4/20.
//

#ifndef DAWNSEEKER_SIMULATE_H
#define DAWNSEEKER_SIMULATE_H

#include "immbasic.h"
#include "thread.h"
#include <future>
#include <numeric>
#include <queue>

struct SimResultItem {
    double positiveGain;    // positive = sum of all the gain(v) > 0
    double negativeGain;    // negative = sum of all the gain(v) < 0
    double totalGain;       // total = positive + negative

    // Count of nodes of each state
    double noneCount;
    double caPlusCount;
    double caCount;
    double crCount;
    double crMinusCount;

    static const auto& members() {
        static auto mps = std::initializer_list<std::tuple<double SimResultItem::*, const char*>>{
                {&SimResultItem::positiveGain, "positiveGain"},
                {&SimResultItem::negativeGain, "negativeGain"},
                {&SimResultItem::totalGain, "totalGain"},

                {&SimResultItem::noneCount, "count of None"},
                {&SimResultItem::caPlusCount, "count of Ca+"},
                {&SimResultItem::caCount, "count of Ca"},
                {&SimResultItem::crCount, "count of Cr"},
                {&SimResultItem::crMinusCount, "count of Cr-"}
        };
        return mps;
    }

    void add(double value, NodeState state) {
        totalGain += value;
        if (value > 0.0) {
            positiveGain += value;
        } else {
            negativeGain += value;
        }

        switch (state) {
        case NodeState::CaPlus:
            caPlusCount += 1;
            break;
        case NodeState::Ca:
            caCount += 1;
            break;
        case NodeState::Cr:
            crCount += 1;
            break;
        case NodeState::CrMinus:
            crMinusCount += 1;
            break;
        default:
            noneCount += 1;
            break;
        }
    }

    void add(NodeState state) {
        add(gain(state), state);
    }

    SimResultItem& operator += (const SimResultItem& rhs) {
        for (auto mp: members() | vs::keys) {
            this->*mp += rhs.*mp;
        }
        return *this;
    }

    SimResultItem operator + (const SimResultItem& rhs) const {
        auto res = *this;
        res += rhs;
        return res;
    }

    SimResultItem& operator /= (std::size_t n) {
        for (auto mp: members() | vs::keys) {
            this->*mp /= (double)n;
        }
        return *this;
    }

    SimResultItem operator / (std::size_t n) const {
        auto res = *this;
        res /= n;
        return res;
    }

    SimResultItem& operator -= (const SimResultItem& rhs) {
        for (auto mp: members() | vs::keys) {
            this->*mp -= rhs.*mp;
        }
        return *this;
    }

    SimResultItem operator - (const SimResultItem& rhs) const {
        auto res = *this;
        res -= rhs;
        return res;
    }
};

inline std::string toString(const SimResultItem& item, int indent = 4) {
    indent = std::max(indent, 0);
    auto indentStr = std::string(indent, ' ');

    auto res = std::string("{\n");
    for (auto [mp, name]: SimResultItem::members()) {
        res += format("{}{}: {:.3f}\n", indentStr, name, item.*mp);
    }
    res += "}";
    return res;
}

struct SimResult {
    SimResultItem withBoosted;
    SimResultItem withoutBoosted;
    SimResultItem diff;     // diff = without boosted - without boosted
};

inline std::string toString(const SimResult& res) {
    return format("with boosted: {},\nwithout boosted: {},\ndiff: {}",
                  res.withBoosted, res.withoutBoosted, res.diff);
}

/*!
 * @brief Properties of a node during simulation
 */
struct NodeSimProperties {
    static constexpr auto infDist = utils::halfMax<std::size_t>;

    // State of current node (None by default)
    NodeState   state   = NodeState::None;
    // Distance from seed nodes (+inf by default)
    std::size_t dist    = infDist;
    // Whether the node is boosted (false by default)
    bool        boosted = false;
};

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
 * The link states object (which must be initialized with graph size |E|) is provided for reusing.
 * Refreshing is done once during this procedure.
 *
 * The node states list is provided for reusing.
 *
 * @param graph The whole graph
 * @param linkStates The already initialized link states object
 * @param node The list of node property collection for each node
 * @param seeds The seed set
 * @param boostedNodes The list of boosted nodes
 * @return A SimResultItem object with the result of this simulation.
 */
template <rs::range Range>
inline SimResultItem simulateBoostedOnce(
        const IMMGraph&                 graph,
        IMMLinkStateSamples&            linkStates,
        std::vector<NodeSimProperties>& nodes,
        const SeedSet&                  seeds,
        Range&&                         boostedNodes)
requires(std::convertible_to<rs::range_value_t<Range>, std::size_t>) {
    // First refreshes all the link states
    linkStates.initOrRefresh(graph.nLinks());
    // Initializes all the node properties with default values
    nodes.assign(graph.nNodes(), NodeSimProperties{});

    // Marks all the boosted nodes
    for (auto s: boostedNodes) {
        nodes[s].boosted = true;
    }

    auto Q = std::queue<std::size_t>();
    // Adds all the seeds to queue first
    for (auto a: seeds.Sa()) {
        nodes[a].state = NodeState::Ca;
        nodes[a].dist = 0;
        Q.emplace(a);
    }
    for (auto r: seeds.Sr()) {
        nodes[r].state = NodeState::Cr;
        nodes[r].dist = 0;
        Q.emplace(r);
    }

    for (; !Q.empty(); Q.pop()) {
        auto cur = Q.front();
        // Boosted nodes may change its state,
        //  making positive message boosted and negative message neutralized
        if (nodes[cur].boosted) {
            if (nodes[cur].state == NodeState::Ca) {
                nodes[cur].state = NodeState::CaPlus;
            } else if (nodes[cur].state == NodeState::Cr) {
                nodes[cur].state = NodeState::CrMinus;
            }
        }
        for (const auto& [to_, link]: graph.fastLinksFrom(cur)) {
            auto to = index(to_);
            // Checks the link state
            // For Ca+ message, either boosted or active is OK.
            if (nodes[cur].state == NodeState::CaPlus && linkStates.get(link) == LinkState::Blocked) {
                continue;
            }
            // For others, only active.
            if (nodes[cur].state != NodeState::CaPlus && linkStates.get(link) != LinkState::Active) {
                continue;
            }

            if (nodes[cur].dist + 1 < nodes[to].dist) {
                // If to is never visited before, adds it to queue
                if (nodes[to].dist == NodeSimProperties::infDist) {
                    Q.emplace(to);
                }
                // Updates dist and state, message propagates along cur -> to
                nodes[to].state = nodes[cur].state;
                nodes[to].dist = nodes[cur].dist + 1;
            }
                // Some other message has arrived in the same round, but current one has higher priority
            else if (nodes[cur].dist + 1 == nodes[to].dist && compare(nodes[cur].state, nodes[to].state) > 0) {
                nodes[to].state = nodes[cur].state;
            }
        }
    }

    auto res = SimResultItem{};
    for (const auto& node: graph.nodes()) {
        res.add(nodes[index(node)].state);
    }
    return res;
}

/*!
 * @brief Simulates message propagation with given boosted nodes.
 *
 * See simulateBoostedOnce(graph, linkStates, nodes, seeds, boostedNodes) for details.
 *
 * @param graph The whole graph
 * @param seeds The seed set
 * @param boostedNodes The list of boosted nodes
 * @return A SimResultItem object with the result of this simulation.
 */
template <rs::range Range>
inline SimResultItem simulateBoostedOnce(
        const IMMGraph&                 graph,
        const SeedSet&                  seeds,
        Range&&                         boostedNodes) {
    auto linkStates = IMMLinkStateSamples(graph.nLinks());
    auto nodes = std::vector<NodeSimProperties>(graph.nNodes());
    simulateBoostedOnce(graph, linkStates, nodes, seeds, std::forward<Range>(boostedNodes));
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
template <rs::range Range> requires (std::convertible_to<rs::range_value_t<Range>, std::size_t>)
SimResultItem simulateBoosted(
        IMMGraph&       graph,
        const SeedSet&  seeds,
        Range&&         boostedNodes,
        std::size_t     simTimes,
        std::size_t     nThreads = 1) {
    // Reuses link state objects for each thread
    auto linkStatesPool = std::vector<IMMLinkStateSamples>{nThreads};
    for (std::size_t i = 0; i < nThreads; i++) {
        linkStatesPool[i].init(graph.nLinks());
    }
    // Reuses node state lists for each thread
    auto nodesPool = std::vector<std::vector<NodeSimProperties>>{nThreads};
    // Results of each thread
    auto subResults = std::vector<SimResultItem>{nThreads};

    runTaskGroup(vs::iota(std::size_t{0}, nThreads) | vs::transform([&](std::size_t tid) {
        return [&, tid](std::size_t) {
            auto& linkStates    = linkStatesPool[tid];
            auto& nodes         = nodesPool[tid];
            auto& subRes        = subResults[tid];
            subRes += simulateBoostedOnce(graph, linkStates, nodes, seeds, boostedNodes);
        };
    }), vs::iota(std::size_t{0}, simTimes));

    return std::accumulate(subResults.begin(), subResults.end(), SimResultItem{}) / simTimes;
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
