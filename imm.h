#ifndef DAWNSEEKER_IMM_H
#define DAWNSEEKER_IMM_H

#include "args-v2.h"
#include "global.h"
#include "immbasic.h"
#include "utils.h"
#include <map>

/*!
 * @brief Result for single sample size of IMM algorithm.
 *
 * Including:
 *   - List of boosted nodes
 *   - Total gain in greedy selection algorithm
 *   - Statistics on time and space consumption
 */
struct IMMResultItem {
    // List of boosted nodes, sorted in descending order by gain value it makes
    std::vector<std::size_t>    boostedNodes;
    // Total gain in greedy selection sub-problem.
    // totalGain = |V| * E(gains / |R|) where
    //   |V| = graph size,
    //   |R| = sample size,
    //   gains = result of greedy selection sub-problem
    double                      totalGain{};
    // Estimation of time used (in seconds) during algorithm
    double                      timeUsed;
    // Estimation of memory usage (in bytes) during algorithm
    std::size_t                 memoryUsage;
};

/*!
 * @brief Result of IMM algorithm.
 *
 * Contains a map from sample size to the corresponding result.
 * For each sample size during the algorithm, a record (nSamples, item) is added to items,
 * where nSamples = total number of PRR-sketches in PR-IMM,
 * or the number of samples per center node in SA-IMM or SA-RG-IMM.
 */
struct IMMResult {
    std::map<std::uint64_t, IMMResultItem> items;

    /*!
     * @brief Accesses the result item via sample size.
     *
     * An exception is thrown if no result item corresponding to current sample size.
     *
     * @param nSamples Sample size
     * @return The result item
     * @throw std::invalid_argument if no such sample size exists in the map.
     */
    const IMMResultItem& operator [] (std::uint64_t nSamples) {
        auto it = items.find(nSamples);
        if (it == items.end()) {
            throw std::invalid_argument(format("Invalid sample size {}", nSamples));
        }
        return it->second;
    }
};

/*!
 * @brief Result group of SA-IMM or SA-RG-IMM algorithm.
 *
 * Including:
 *   - At most 3 results for the objective function and/or its lower bound and/or upper bound resp.
 *   - Labels of the 3 results resp.
 */
struct IMMResult3 : std::array<IMMResult, 3> {
    std::array<std::string, 3> labels;
};

/*!
 * @brief Result of greedy or other naive algorithms.
 *
 * Including:
 *   - List of boosted nodes, sorted in descending order by the sort key of the corresponding algorithm.
 */
struct GreedyResult {
    std::vector<std::size_t> boostedNodes;
};

/*!
 * @brief Dumps the IMMResultItem object as a multi-line string.
 *
 * The format is as the following example
 * (indentation spaces are replaced as - for clarity, take indentation = 4 as example):
 *
 *      {
 *      ----.totalGain = 1.234,
 *      ----.boostedNodes = {1, 2, 3, 4},
 *      ----.timeUsed = 5.678 sec.
 *      ----.memoryUsage = 4096 bytes = 4.000 KibiBytes
 *      }
 *
 * Indentation size should be non-negative. If a negative value is provided, it's treated as 0.
 *
 * @param item The IMMResultItem object
 * @param indent Indentation width
 * @return A multi-line string, without trailing new-line character.
 */
inline std::string toString(const IMMResultItem& item, int indent = 4) {
    indent = std::max(0, indent);

    auto indentStr = std::string(indent, ' ');
    auto res = std::string{"{\n"};

    res += indentStr + format(".boostedNodes = {},\n", join(item.boostedNodes, ", ", "{", "}"));
    res += indentStr + format(".totalGain = {},\n",
                              (item.totalGain <= halfMin<double> ? "-inf" : toString(item.totalGain, 'f', 3)));
    res += indentStr + format(".timeUsed = {:.3f} sec.\n", item.timeUsed);
    res += indentStr + format(".memoryUsage = {}\n", utils::totalBytesUsedToString(item.memoryUsage));

    res += "}";
    return res;
}

/*!
 * @brief Solves with PR-IMM algorithm with dynamic sample size. For monotonic & sub-modular cases only.
 *
 * The result contains only one record: final sample size => result item.
 *
 * @param graph The whole graph
 * @param seeds The seed set
 * @param args Arguments of the algorithm
 * @return an IMMResult object as the algorithm result
 */
IMMResult PR_IMM_Dynamic(IMMGraph& graph, const SeedSet& seeds, const DynamicArgs_PR_IMM& args);


/*!
 * @brief Solves with PR-IMM algorithm with fixed sample sizes. For monotonic & sub-modular cases only.
 *
 * The result contains several records, sample size => result item for each nSamples in args.nSamplesList.
 *
 * @param graph The whole graph
 * @param seeds The seed set
 * @param args Arguments of the algorithm
 * @return an IMMResult object as the algorithm result
 */
IMMResult PR_IMM_Static(IMMGraph& graph, const SeedSet& seeds, const StaticArgs_PR_IMM& args);

/*!
 * @brief Solves with PR-IMM algorithm. For monotonic & sub-modular cases only.
 *
 * If args is [Dynamic | Static]Args_PR_IMM, returns the result of PR_IMM_[Dynamic | Static];
 * Otherwise, an exception is thrown.
 *
 * @param graph The whole graph
 * @param seeds The seed set
 * @param args Arguments of the algorithm
 * @return an IMMResult object as the algorithm result
 * @throw std::bad_cast if args is neither DynamicArgs_PR_IMM nor StaticArgs
 */
IMMResult PR_IMM(IMMGraph& graph, const SeedSet& seeds, const BasicArgs& args);

/*!
 * @brief Solves the lower bound part of SA-IMM or SA-RG-IMM algorithm with dynamic sample size.
 *
 * The result contains only one record: final sample size => result item.
 *
 * @param graph The whole graph
 * @param seeds The seed set
 * @param args Arguments of the algorithm
 * @return an IMMResult object as the algorithm result
 */
IMMResult SA_IMM_LB_Dynamic(IMMGraph& graph, const SeedSet& seeds, const DynamicArgs_SA_IMM_LB& args);

/*!
 * @brief Solves the lower bound part of SA-IMM or SA-RG-IMM algorithm with fixed sample sizes.
 *
 * The result contains several records, sample size => result item for each nSamples in args.nSamplesList.
 *
 * @param graph The whole graph
 * @param seeds The seed set
 * @param args Arguments of the algorithm
 * @return an IMMResult object as the algorithm result
 */
IMMResult SA_IMM_LB_Static(IMMGraph& graph, const SeedSet& seeds, const StaticArgs_SA_IMM_LB& args);

/*!
 * @brief Solves the lower bound part of SA-IMM or SA-RG-IMM algorithm.
 *
 * If args is [Dynamic | Static]Args_SA_IMM_LB, returns the result of SA_IMM_LB_[Dynamic | Static];
 * Otherwise, an exception is thrown.
 *
 * @param graph The whole graph
 * @param seeds The seed set
 * @param args Arguments of the algorithm
 * @return an IMMResult object as the algorithm result
 * @throw std::bad_cast if args is neither DynamicArgs_PR_IMM nor StaticArgs
 */
IMMResult SA_IMM_LB(IMMGraph& graph, const SeedSet& seeds, const BasicArgs& args);

/*!
 * @brief Solves with SA-IMM (for monotonic cases only) & SA-RG-IMM algorithm. For non-submodular cases.
 *
 * The result contains two parts:
 *   - Upper bound: performs PR-IMM algorithm on a "upper bound" node state priority
 *   - Lower bound: performs SA-IMM-LB algorithm. See SA_IMM_LB(graph, seeds, args) for details.
 *
 * @param graph The whole graph.
 * @param seeds The seed set.
 * @param args Arguments of the algorithm
 * @return an IMMResult3 object as the algorithm result.
 * @throw std::bad_cast if conversion of argument type fails.
 */
IMMResult3 SA_IMM(IMMGraph& graph, const SeedSet& seeds, const BasicArgs& args);

/*!
 * @brief Solves with greedy algorithm.
 *
 * Performs greedy algorithm as the following pseudocode:
 *
 *      input:
 *          G(V, E) as the graph
 *          k as the budget
 *          S_a and S_r as seed sets of positive and negative message resp.
 *          T as the number of repeat times during each simulation
 *      output:
 *          S as the selected boosted node set such that |S| <= k
 *      S = {}
 *      for i = 1 to k:
 *          for v in V - S_a - S_r - S
 *              // Simulates T times and takes the average
 *              gain(v) = simulate(G, S + {v}, T).totalGain
 *          S += { argmax_v gain(v) }
 *      return S
 *
 * @param graph The whole graph
 * @param seeds The seed set
 * @param args Arguments of the algorithm
 * @return A GreedyResult object as the algorithm result.
 * @throw std::bad_cast if args is not GreedyArgs
 */
GreedyResult greedy(IMMGraph& graph, const SeedSet& seeds, const BasicArgs& args);

/*!
 * @brief Solves with max-degree algorithm.
 *
 * Simply picks the nodes with maximum degree.
 *
 * @param graph The whole graph
 * @param seeds The seed set
 * @param args Arguments of the algorithm
 * @return A GreedyResult object as the algorithm result.
 */
GreedyResult maxDegree(IMMGraph& graph, const SeedSet& seeds, const BasicArgs& args);

/*!
 * @brief Solves with PageRank algorithm.
 *
 * Simply picks the nodes with maximum PageRanks.
 *
 * @param graph The whole graph
 * @param seeds The seed set
 * @param args Arguments of the algorithm
 * @return A GreedyResult object as the algorithm result.
 */
GreedyResult pageRank(IMMGraph& graph, const SeedSet& seeds, const BasicArgs& args);

/*!
 * @brief Dumps the IMMResult3 object as a multi-line string.
 *
 * The format is as the following example
 * (indentation spaces are replaced as - for clarity, take indentation = 4 as example):
 *
 *      {
 *      ----'Lower bound': {
 *      --------.totalGain = 1.234,
 *      --------.boostedNodes = {1, 2, 3, 4}
 *      ----},
 *      ----'Upper bound': {
 *      --------.totalGain = 1.432,
 *      --------.boostedNodes = {1, 2, 3, 5}
 *      ----},
 *      ----'Objective function': {
 *      --------.totalGain = 1.333,
 *      --------.boostedNodes = {1, 2, 4, 5}
 *      ----}
 *      }
 *
 * Indentation size should be non-negative. If a negative value is provided, it's treated as 0.
 *
 * @param item The IMMResult3 object
 * @param indent Indentation width
 * @return A multi-line string, without trailing new-line character
 */
//inline std::string toString(const IMMResult3& item, int indent = 4) {
//    // Negative indentation is treated as 0
//    indent = std::max(0, indent);
//
//    auto res = std::string("{");
//    for (std::size_t i = 0; i < 3; i++) {
//        res += "\n" + std::string(indent, ' ');
//        res += format("'{0}': {1}", item.labels[i], toString(item[i], indent));
//        // Removes the trailing '}' temporarily
//        res.pop_back();
//        // And then adds it back with indentation
//        res += std::string(indent, ' ') + '}';
//        // Appends comma after the first 2 IMMResultItem string representations.
//        if (i < 2) {
//            res += ',';
//        }
//    }
//    res += "\n}";
//    return res;
//}

#endif //DAWNSEEKER_IMM_H