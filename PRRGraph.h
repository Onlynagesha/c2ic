#ifndef DAWNSEEKER_PRRGRAPH_H
#define DAWNSEEKER_PRRGRAPH_H

#include "graphbasic.h"
#include "immbasic.h"

/*!
 * @brief Creates a sample of PRR-sketch, with given seed set and center as initial node.
 *
 * For each node v in the PRR-sketch, the following properties will be set:
 *   - v.dist:  the minimum distance from any seed node to v;
 *   - v.state: the state of v without any boosted node (as one of Ca, Cr or None)
 *
 * For each link e =  u -> v in the PRR-sketch, the following properties will be set:
 *   - e.state: the link state (Active or Boosted)
 *
 * @param graph The whole graph
 * @param seeds The seed set
 * @param center The center node of current PRR-sketch
 * @return The PRR-sketch object.
 */
PRRGraph samplePRRSketch(const IMMGraph& graph, const SeedSet& seeds, std::size_t center);

/*!
 * @brief Creates a sample of PRR-sketch on the given object, with given seed set and center as initial node.
 *
 * The result will be written to prrGraph object, with old contents cleared.
 * The destination prrGraph object must be preserved before calling, with the following arguments:
 *   - "nodes" = |V|
 *   - "links" = |E|
 *   - "maxIndex" = |V|
 *
 * WARNING on multithreading cases: different prrGraph objects for different threads.
 * The result will be incorrect or the program may crash
 * if multiple threads attempt to write to the same PRR-sketch object.
 *
 * See samplePRRSketch(graph, seeds, center) for details.
 *
 * @param graph The whole graph
 * @param prrGraph The destination PRR-sketch object
 * @param seeds The seed set
 * @param center The center node of current PRR-sketch
 */
void samplePRRSketch(const IMMGraph& graph, PRRGraph& prrGraph, const SeedSet& seeds, std::size_t center);

/*!
 * @brief Creates a sample of PRR-sketch on the given object, with given seed set, center and link state object.
 *
 * The link state object must be initialized before calling, with n = the graph size |V|.
 * Its contents will be refreshed and updated.
 *
 * WARNING on multithreading cases: different linkStates objects for different threads.
 * The result will be incorrect or the program may crash
 * if multiple threads attempt to write to the same linkStates object.
 *
 * See samplePRRSketch(graph, prrGraph, seeds, center) for details.
 *
 * @param graph The whole graph
 * @param prrGraph The destination PRR-sketch object
 * @param linkStates The already-initialized link states object
 * @param seeds The seed set
 * @param center The center node of current PRR-sketch
 */
void samplePRRSketch(const IMMGraph&        graph,
                     IMMLinkStateSamples&   linkStates,
                     PRRGraph&              prrGraph,
                     const SeedSet&         seeds,
                     std::size_t            center);

/*!
 * @brief Calculates gain(v; prrGraph) for each v in prrGraph. FOR MONOTONE & SUB-MODULAR CASES ONLY.
 *
 * gain(v; prrGraph) = difference of state of the center node
 *   between setting S = {v} as the boost node set and S = {}.
 * If gain(v; prrGraph, center) > 0, it indicates that setting v as a boosted node
 *   may improve the result.
 *
 * For each node v, the following properties of v is set:
 *   - v.centerStateTo: Which states the center node will become if v is set as boosted.
 *   - v.distR
 *   - v.maxDistP
 *
 * gain(v; G) is implied as gain(v.centerStateTo) - gain(G.centerState).
 *
 * Time complexity: O(E_r log V_r) where V_r, E_r = number of nodes and links in the PRR-sketch
 *
 * WARNING on multithreading cases: different prrGraph objects for different threads.
 * The result will be incorrect or the program may crash
 * if multiple threads attempt to write to the same PRR-sketch object.
 *
 * @param prrGraph The PRR-sketch object.
 */
void calculateCenterStateToFast(PRRGraph& prrGraph);

/*!
 * @brief Calculates gain(v; prrGraph) for each v in prrGraph.
 *
 * For each node v, the following properties of v is set:
 *   - v.centerStateTo: Which states the center node will become if v is set as boosted.
 *     (see calculateCenterStateToFast for details)
 *
 * Time complexity: O(V_r * E_r) where V_r (E_r) = number of nodes (links) in the PRR-sketch.
 * This version has no constraints on monotonicity or sub-modularity but is much slower.
 *
 * WARNING on multithreading cases: different prrGraph objects for different threads.
 * The result will be incorrect or the program may crash
 * if multiple threads attempt to write to the same PRR-sketch object.
 *
 * @param prrGraph The PRR-sketch object
 */
void calculateCenterStateToSlow(PRRGraph& prrGraph);

#endif 