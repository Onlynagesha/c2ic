#ifndef DAWNSEEKER_PRRGRAPH_H
#define DAWNSEEKER_PRRGRAPH_H

#include "immbasic.h"
#include <type_traits>

struct PRRNode : graph::BasicNode<std::size_t> {
    int	        dist = 0;
    int         distR = 0;
    int         maxDistP = 0;
    NodeState   state = NodeState{};
    double      gain = 0.0;

    PRRNode() = default;

    explicit PRRNode(std::size_t index, int dist = 0, NodeState state = NodeState::None) :
        BasicNode(index), dist(dist), state(state) {}
};

using PRRGraph = graph::Graph<
        PRRNode, IMMLink,
        graph::SemiSparseIndexMap,
        graph::ReserveStrategy::Nodes | graph::ReserveStrategy::Links | graph::ReserveStrategy::MaxIndex
        >;

/*
* Creates a sample of PRR-sketch, with given seed set and center as initial node
* All the links in the resulting PRR-sketch are either Active or Boosted
*   (i.e. Blocked links are filtered out)
*/
PRRGraph samplePRRSketch(IMMGraph& graph, const SeedSet& seeds, std::size_t center);

/*
* Forward simulation of message propagation with no boosted nodes.
* Sets the state of all the visited nodes to either Ca or Cr
* Note that some nodes may not be visited, whose states are left as None
*/
void simulateNoBoost(PRRGraph& prrGraph, const SeedSet& seeds);

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
void calculateGainFast(PRRGraph& prrGraph, std::size_t center);

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
void calculateGainSlow(PRRGraph& prrGraph, std::size_t center);

#endif 