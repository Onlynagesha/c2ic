#ifndef DAWNSEEKER_PRRGRAPH_H
#define DAWNSEEKER_PRRGRAPH_H

#include "immbasic.h"

// Node type in the PRR-sketch
struct PRRNode : graph::BasicNode<std::size_t> { // NOLINT(cppcoreguidelines-pro-type-member-init)
    // Which state this node will become if no boosted node exists
    // Ca, Cr, or None, according to which kind of message comes first
    NodeState   state = NodeState{};
    // Which state this node will change the center node of its PRR-sketch to
    //  if this node is set as a boosted node
    NodeState   centerStateTo = NodeState{};
    // Minimum distance from any seed node
    int	        dist = 0;
    // Reversed minimum distance from the center node
    int         distR = 0;
    // For all Ca nodes, maxDistP = maximal accepted distance from the nearest seed node
    //  with which this node changes the center node to Ca+ after being boosted.
    int         maxDistP = 0;

    PRRNode() = default;

    explicit PRRNode(std::size_t index, int dist = 0) :
        BasicNode(index), dist(dist) {}
};

using PRRGraphBase = graph::Graph<
        PRRNode, IMMLink,
        graph::LinearIndexMap,
        graph::tags::EnablesFastAccess::Yes
>;

// Graph type of the PRR-sketch
struct PRRGraph: PRRGraphBase {
    // Index of the center node
    std::size_t center{};
    // State of the center node, equivalent to .centerNode().state
    NodeState   centerState{};

    // Delayed reservation
    explicit PRRGraph(graph::tags::ReservesLater later): PRRGraphBase(later) {}
    // Reserves WITH the arguments
    explicit PRRGraph(const std::map<std::string, std::size_t>& reserveArgs): PRRGraphBase(reserveArgs) {}

    PRRNode& centerNode() {
        return operator[](center);
    }

    [[nodiscard]] const PRRNode& centerNode() const {
        return operator[](center);
    }
};

/*
* Creates a sample of PRR-sketch, with given seed set and center as initial node
* All the links in the resulting PRR-sketch are either Active or Boosted
*   (i.e. Blocked links are filtered out)
* This procedure will set:
*   v.dist:  the minimum distance from any seed node to v;
*   v.state: the state of v without any boosted node (as one of Ca, Cr or None)
*/
PRRGraph samplePRRSketch(const IMMGraph& graph, const SeedSet& seeds, std::size_t center);

/*
 * Creates a sample of PRR-sketch and writes the result to prrGraph
 * The PRRGraph object must have been reserved well.
 * Reserve arguments should contain: {
 *   { "nodes", |V| },
 *   { "links", |E| },
 *   { "maxIndex", |V| }
 * } where |V| and |E| are the number of nodes and indices of the whole graph respectively
 */
void samplePRRSketch(const IMMGraph& graph, PRRGraph& prrGraph, const SeedSet& seeds, std::size_t center);

/*
* Calculates gain(v; prrGraph, center) for each v in prrGraph
* gain(v; prrGraph, center) = difference of state of the center node
*   between setting S = {v} as the boost node set and S = {}
* Sets v.centerStateTo and gain of v is implies as gain(v.centerStateTo) - gain(G.centerState)
* If gain(v; prrGraph, center) > 0, it indicates that setting v as a boosted node
*   may improve the result.
*
* ** FOR MONOTONE & SUB-MODULAR CASES ONLY **
* Time complexity: O(E_r log V_r) where V_r, E_r = number of nodes and links in the PRR-sketch
*/
void calculateCenterStateToFast(PRRGraph& prrGraph);

/*
* Calculates gain(v; prrGraph, center) for each v in prrGraph
* gain(v; prrGraph, center) = difference of state of the center node
*   between setting S = {v} as the boost node set and S = {}
* Sets v.centerStateTo and gain of v is implies as gain(v.centerStateTo) - gain(G.centerState)
*
* This version has no constraints on monotonicity or sub-modularity
*   but is much slower than the calculateCenterStateToFast version
* Time complexity: O(V_r * E_r) 
*   where V_r (E_r) = number of nodes (links) in the PRR-sketch
*/
void calculateCenterStateToSlow(PRRGraph& prrGraph);

#endif 