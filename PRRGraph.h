#ifndef DAWNSEEKER_PRRGRAPH_H
#define DAWNSEEKER_PRRGRAPH_H

#include "immbasic.h"
#include <type_traits>

struct PRRNode : graph::BasicNode<std::size_t> { // NOLINT(cppcoreguidelines-pro-type-member-init)
    NodeState   state = NodeState{};
    NodeState   centerStateTo = NodeState{};
    int	        dist = 0;
    int         distR = 0;
    int         maxDistP = 0;

    PRRNode() = default;

    explicit PRRNode(std::size_t index, int dist = 0) :
        BasicNode(index), dist(dist) {}
};

using PRRGraphBase = graph::Graph<
        PRRNode, IMMLink
        ,graph::SemiSparseIndexMap
        ,graph::ReserveStrategy::All
>;

struct PRRGraph: PRRGraphBase {
    std::size_t center;
    NodeState   centerState;

#pragma clang diagnostic push
#pragma ide diagnostic ignored "cppcoreguidelines-pro-type-member-init"
    explicit PRRGraph(graph::tags::ReservesLater later): PRRGraphBase(later) {}
    explicit PRRGraph(const graph::ReserveArguments& reserveArgs): PRRGraphBase(reserveArgs) {}
    explicit PRRGraph(const std::map<std::string, std::size_t>& reserveArgs): PRRGraphBase(reserveArgs) {}
#pragma clang diagnostic pop

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
*/
PRRGraph samplePRRSketch(IMMGraph& graph, const SeedSet& seeds, std::size_t center);

/*
 * Creates a sample of PRR-sketch and writes the result to dest
 * The PRRGraph object must have been reserved well
 */
void samplePRRSketch(IMMGraph& graph, PRRGraph& prrGraph, const SeedSet& seeds, std::size_t center);

/*
* Calculates gain(v; prrGraph, center) for each v in prrGraph
* Sets v.gain for each v
* gain(v; prrGraph, center) = difference of state of the center node
*   between setting S = {v} as the boost node set and S = {}
* If gain(v; prrGraph, center) > 0, it indicates that setting v as a boosted node
*   may improve the result.
* ** FOR MONOTONE & SUB-MODULAR CASES ONLY **
* Time complexity: O(E_r) where E_r = number of links in the PRR-sketch
*/
void calculateGainFast(PRRGraph& prrGraph);

/*
* Calculates gain(v; prrGraph, center) for each v in prrGraph
* Sets v.gain for each v
* gain(v; prrGraph, center) = difference of state of the center node
*   between setting S = {v} as the boost node set and S = {}
* This version has no constraints on monotonicity or sub-modularity
*   but is much slower than the calculateGainFast version
* Time complexity: O(V_r * E_r) 
*   where V_r (E_r) = number of nodes (links) in the PRR-sketch
*/
void calculateGainSlow(PRRGraph& prrGraph);

#endif 