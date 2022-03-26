#include "global.h"
#include "Logger.h"
#include "PRRGraph.h"
#include <cassert>
#include <queue>
#include <vector>

/*
* Step 1: get limitDist
* prrGraph is an empty graph initially.
* prrGraph should be preserved
*/
int getLimitDist(PRRGraph& prrGraph, IMMGraph& graph, const SeedSet& seeds, std::size_t center) 
{
    auto Q = std::queue<std::size_t>({ center });
    // Add initial node
    prrGraph.emplaceNode(center);

    for (; !Q.empty(); Q.pop()) {
        auto cur = Q.front();
        // Traverse in the transposed graph
        for (auto [from, e] : graph.fastLinksTo(cur)) {
            auto next = index(from);
            // Consider only Active links
            // Here we consider the case with no boosted nodes, 
            //  thus boosted links are regarded as blocked as well
            if (e.getState() != LinkState::Active || prrGraph.hasNode(next)) {
                continue;
            }
            int nextDist = prrGraph[cur].dist + 1;
            prrGraph.emplaceNode(next, nextDist);
            Q.push(next);
            // Stops when meeting any seed node
            if (seeds.contains(next)) {
                return nextDist;
            }
        }
    }
    // If all the seeds are unreachable from center via inverse active links,
    //  then return a sufficiently large limit
    return (int) graph.nNodes();
}

/*
* Step 3: Forward simulation of message propagation with no boosted nodes.
*   Sets the state of all the visited nodes to either Ca or Cr
*   Note that some nodes may not be visited, whose states are left as None
*/
void simulateNoBoost(PRRGraph& prrGraph, const SeedSet& seeds)
{
    // Initialize distance to infinity, and state to None
    for (auto& node : prrGraph.nodes()) {
        node.state = NodeState::None;
        node.dist = halfMax<int>;
    }

    auto Q = std::queue<PRRNode*>();

    auto initSeeds = [&](const auto& seeds, NodeState state) {
        for (auto a : seeds) {
            if (auto node = prrGraph.node(a); node != nullptr) {
                node->dist = 0;
                node->state = state;
                Q.emplace(node);
            }
        }
    };
    // Initializes the queue with higher priority seeds first,
    //  lower priority seeds then.
    if ((NodeState::Ca <=> NodeState::Cr) == std::strong_ordering::greater) {
        initSeeds(seeds.Sa(), NodeState::Ca);
        initSeeds(seeds.Sr(), NodeState::Cr);
    }
    else {
        initSeeds(seeds.Sr(), NodeState::Cr);
        initSeeds(seeds.Sa(), NodeState::Ca);
    }

    //    High priority with dist = 0
    // -> Low priority with dist = 0
    // -> High priority with dist = 1
    // -> Low priority with dist = 1
    // ...
    for (; !Q.empty(); Q.pop()) {
        auto& cur = *Q.front();
        for (auto [to, e] : prrGraph.fastLinksFrom(cur)) {
            // Consider Active links only
            if (e.forceGetState() != LinkState::Active) {
                continue;
            }
            // If never visited before
            if (to.dist == halfMax<int>) {
                to.dist = cur.dist + 1;
                to.state = cur.state;
                Q.emplace(&to);
            }
        }
    }
    // Marks the state of center node
    prrGraph.centerState = prrGraph.centerNode().state;
}

/*
* Step 2: get the PRR-sketch sub-graph, all the nodes within limitDist
*/
void samplePRRSketch(IMMGraph& graph, PRRGraph& prrGraph, const SeedSet& seeds, std::size_t center)
{
    IMMLink::refreshAllStates();

    prrGraph.reserveClear();
    prrGraph.center = center;
    auto limitDist = getLimitDist(prrGraph, graph, seeds, center);

    // Step 2: Do PRR-sketch, with boosted links into consideration
    // Do BFS for limitDist steps
    auto Q = std::queue<std::size_t>({ center });
    prrGraph.reserveClear();
    prrGraph.emplaceNode(center);

    for (; !Q.empty(); Q.pop()) {
        auto cur = Q.front();
        auto nextDist = prrGraph[cur].dist + 1;
        // Traverse in the transposed graph
        for (auto [from, e] : graph.fastLinksTo(cur)) {
            auto next = index(from);
            if (e.getState() == LinkState::Blocked) {
                continue;
            }
            // Add nodes to the sketched PRR-subgraph
            if (!prrGraph.hasNode(next)) {
                prrGraph.emplaceNode(next, nextDist);
                // Only nodes within limitDist is considered
                if (nextDist < limitDist) {
                    Q.push(next);
                }
            }
            // Add the link next -> cur
            // The link is either Active or Boosted
            prrGraph.fastAddLink(e);
        }
    }
    // Step 3: forward simulation
    simulateNoBoost(prrGraph, seeds);
}

PRRGraph samplePRRSketch(IMMGraph& graph, const SeedSet& seeds, std::size_t center) 
{
    // Creates an empty graph
    auto prrGraph = PRRGraph({
        {"maxIndex", graph.nNodes()},
        {"links", graph.nLinks()},
        {"nodes", graph.nNodes()}
    });
    samplePRRSketch(graph, prrGraph, seeds, center);
    return prrGraph;
}

/*
* Considers only the case where the state of center node is Cr
* Check all the nodes with state Cr, and attempt to set it as Cr-
* Requires: centerNode.state == Cr and all gain values are reset as 0.0
*/
void calculateGainFastR(PRRGraph& prrGraph)
{
    graph::NodeOrIndex auto& centerNode = prrGraph.centerNode();
    // Initializes distR to inf
    for (auto& node : prrGraph.nodes()) {
        node.distR = halfMax<int>; 
    }
    // Initializes the queue as {center}
    //  and distR of center as 0
    auto Q = std::queue<PRRNode*>({ &centerNode });
    centerNode.distR = 0;

    // Calculate distR
    for (; !Q.empty(); Q.pop()) {
        auto& cur = *Q.front();
        // Traverse the transposed graph
        for (auto [from, e] : prrGraph.fastLinksTo(cur)) {
            // For negative messages, only the active (but not boosted) are considered
            if (e.forceGetState() == LinkState::Active && from.distR == halfMax<int>) {
                from.distR = cur.distR + 1;
                Q.push(&from);
            }
        }
    }
    for (auto& node : prrGraph.nodes()) {
        // Monotonicity & Sub-modularity assure that Cr- > Cr
        //  and no state between Cr- and Cr
        if (node.state == NodeState::Cr && node.dist + node.distR <= centerNode.dist) {
            // gain = gain(Cr-) - gain(Cr) = 0 - gain(Cr) = 1 - lambda
            node.centerStateTo = NodeState::CrMinus;
        }
    }
}

/*
* Step 4
*/
void calculateGainFast(PRRGraph& prrGraph)
{
    graph::NodeOrIndex auto& centerNode = prrGraph.centerNode();
    // Resets gain of each node to zero
    for (auto& node : prrGraph.nodes()) {
        node.centerStateTo = centerNode.state;
    }
    // Due to monotonicity, if center node has state Ca,
    //  then boosting can not make its state any better
    // If some boosting node changes center's state to Ca+,
    //  no gain is available (since gain(Ca+) = gain(Ca))
    // If changes center's state to Cr- or Cr,
    //  monotonicity is violated due to lower gain (0 or -(1-lambda))
    if (centerNode.state == NodeState::Ca) {
        return;
    }
    // Consider the case where state of center is Cr
    // Check all the nodes with state Cr, and attempt to set it as Cr-
    if (centerNode.state == NodeState::Cr) {
        calculateGainFastR(prrGraph);
    }

    bool crHigher = (NodeState::Cr <=> NodeState::CaPlus) == std::strong_ordering::greater;
    
    // Initializes all the maxDistP (including center node) as inf
    for (auto& node : prrGraph.nodes()) {
        node.maxDistP = halfMax<int>;
    }
    // As the default order in std::priority_queue, 
    //  nodes with the highest maxDistP is at the top
    static auto compByDistA = [](const PRRNode* A, const PRRNode* B) {
        return A->maxDistP < B->maxDistP; 
    };
    // Initializes the priority-queue with {center}, 
    static auto Q = std::priority_queue<PRRNode*, std::vector<PRRNode*>, decltype(compByDistA)>();
    centerNode.maxDistP = 
        centerNode.dist - (crHigher && centerNode.state == NodeState::Cr);
    Q.push(&centerNode);
    
    // Calculate maxDistP
    for (; !Q.empty(); Q.pop()) {
        auto& cur = *Q.top();
        for (auto [from, e] : prrGraph.fastLinksTo(cur)) {
            // For positive messages, both the active and the boosted are considered
            // It's ensured link state in a PRR-sketch is either Active or Boosted
            if (from.maxDistP == halfMax<int>) {
                from.maxDistP = std::min(
                    cur.maxDistP - 1, 
                    from.dist - (crHigher && from.state == NodeState::Cr)
                );
                Q.push(&from);
            }
        }
    }

    for (auto& node : prrGraph.nodes()) {
        // Monotonicity & Sub-modularity assure that Ca+ > Ca
        if (node.state == NodeState::Ca && node.maxDistP >= node.dist) {
            // gain = gain(Ca+) - gain(oldState)
            node.centerStateTo = NodeState::CaPlus;
        }
    }
}

NodeState _calculateGainSlow(
    PRRGraph prrGraph, std::size_t maxIndex, std::size_t v)
{
    graph::NodeOrIndex auto& vNode = prrGraph[v];
    graph::NodeOrIndex auto& centerNode = prrGraph.centerNode();

    // Boost the node v, Ca -> Ca+, or Cr -> Cr-
    if (vNode.state == NodeState::Ca) {
        vNode.state = NodeState::CaPlus;
    }
    else {
        vNode.state = NodeState::CrMinus;
    }

    // vis[u] = whether the node u has been pushed to the queue
    auto vis = std::vector<bool>(maxIndex + 1);

    auto Q = std::queue<PRRNode*>({&vNode});
    vis[v] = true;

    for (; !Q.empty(); Q.pop()) {
        auto& cur = *Q.front(); 
        auto nextDist = cur.dist + 1;

        for (auto [to, e] : prrGraph.fastLinksFrom(cur)) {
            // The link cur -> e.to is reachable if:
            //  (1) cur is in Ca+ state, thus both Active and Boosted links are accepted
            //  (2) The link is in Active state
            if (cur.state != NodeState::CaPlus && e.forceGetState() != LinkState::Active) {
                continue; 
            }
            // State of the target node may change if:
            //  (1) cur.dist + 1 < e.to.dist, 
            //      i.e. message arrives earlier and manages to replace the old one
            //  (2) cur.dist + 1 == e.to.dist,
            //      but some message with higher priority replaces the old one
            if (nextDist < to.dist ||
                nextDist == to.dist && (cur.state <=> to.state) == std::strong_ordering::greater) {
                // Replace the target's state to the current one
                //  that arrives earlier due to boosting, or has higher priority
                to.dist = nextDist; 
                to.state = cur.state;

                // Try to push the target into the queue
                // Each node enters the queue only once
                if (!vis[to.index()]) {
                    vis[to.index()] = true; 
                    Q.push(&to);
                }
            }
        }
    }

    return centerNode.state;
}

void calculateGainSlow(PRRGraph& prrGraph)
{
    auto maxIndex = rs::max(prrGraph.nodes() | vs::transform(&PRRNode::index));

    for (auto& node : prrGraph.nodes()) {
        if (node.state == NodeState::None) {
            node.centerStateTo = prrGraph.centerNode().state;
            continue; 
        }
        auto prrCopy = prrGraph;
        // Consider each node separately
        node.centerStateTo = _calculateGainSlow(std::move(prrCopy), maxIndex, node.index());
    }
}