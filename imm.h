#pragma clang diagnostic push
#pragma ide diagnostic ignored "bugprone-sizeof-container"
#ifndef DAWNSEEKER_IMM_H
#define DAWNSEEKER_IMM_H

#include "args.h"
#include "global.h"
#include "immbasic.h"
#include "PRRGraph.h"
#include <cmath>
#include <locale>
#include <numbers>
#include <vector> 

/*
* Collection of all PRR-sketches
*/
struct PRRGraphCollection { // NOLINT(cppcoreguidelines-pro-type-member-init)
    struct Node {
        std::size_t index;
        double      gain;
    };

    // Number of nodes
    std::size_t                             n;
    // prrGraph[i] = the i-th PRR-sketch
    // Each sketch is simplified as a list of {v, g} pairs,
    //  indicating that node v makes gain = g in this PRR-sketch
    std::vector<std::vector<Node>>          prrGraph;
    // contrib[v] = list of indices of PRR-sketches
    //  where node v makes non-zero gain
    std::vector<std::vector<std::size_t>>   contrib;
    // totalGain[v] = total gain of the node v
    std::vector<double>                     totalGain;

    PRRGraphCollection() = default;
    explicit PRRGraphCollection(std::size_t n) : n(n), contrib(n), totalGain(n) {}

    void init(std::size_t n) {
        this->n = n;
        contrib.clear();
        contrib.resize(n);
        totalGain.resize(n, 0.0);
    }

    void add(const PRRGraph& G) {
        auto prrList = std::vector<Node>();
        // Index of the PRR-sketch to be added
        auto prrListId = prrGraph.size();

        for (const auto& node : G.nodes()) {
            // Zero-gain nodes are skipped to save memory usage
            if (node.gain == 0.0) {
                return;
            }
            auto nodeId = node.index();
            prrList.push_back(Node{ .index = nodeId, .gain = node.gain });
            contrib[nodeId].push_back(prrListId);
            totalGain[nodeId] += node.gain;
        }

        prrGraph.push_back(std::move(prrList));
    }

    // Selects k boosted nodes with greedy algorithm
    // Returns the total gain contributed by the selected nodes. 
    // Note that the gain value returned is a sum of all the |R| PRR-sketches
    //  and you may need to divide it by |R|.
    // Selection results are written via an output iterator.
    // You can provide iter = nullptr to skip this process.
    // For sub-modular cases, it's ensured the local solution is no worse than
    //  (1 - 1/e) x 100% (63.2% approximately) of the global optimal.
    // ** FOR MONOTONE & SUB-MODULAR CASES ONLY **
    template <class OutIter>
        requires std::output_iterator<OutIter, std::size_t>
    || std::same_as<OutIter, std::nullptr_t>
        double greedySelect(std::size_t k, OutIter iter) const {
        double res = 0.0;
        // Makes a copy of the totalGain[] to updateValues during selection
        auto totalGainCopy = totalGain;
        // covered[i] = whether the i-th PRR-sketch is covered by some selected node
        auto covered = std::vector<bool>(prrGraph.size());

        for (std::size_t i = 0; i < k; i++) {
            // cur = The node chosen in this turn, with highest total gain
            std::size_t cur = rs::max(
                vs::iota(std::size_t{0}, n),
                [&](std::size_t A, std::size_t B) {
                    return totalGainCopy[A] > totalGainCopy[B];
                });
            // Output is only enabled when iter != nullptr
            if constexpr (!std::is_same_v<OutIter, std::nullptr_t>) {
                *iter++ = cur;
            }
            res += totalGainCopy[cur];
            // Sets the total gain of selected node as -inf
            //  so that it can never be selected again
            totalGainCopy[cur] = halfMin<double>;
            // Eliminates the influence of the selected node 
            // For each PRR-sketch that contains the selected node cur,
            //  it is already covered by cur, 
            //  thus all the influences this PRR-sketch provides
            //  should be eliminated.
            for (std::size_t prrId : contrib[cur]) {
                // If covered previously, skip and do not cover again
                if (covered[prrId]) {
                    continue;
                }
                covered[prrId] = true;
                for (auto [i, g] : prrGraph[prrId]) {
                    totalGainCopy[i] -= g;
                }
            }
        }
        return res;
    }

    /*
    * Returns an estimation of bytes used
    */
    [[nodiscard]] std::size_t totalBytesUsed() const {
        auto bytes = sizeof(n); 
        // Total bytes of prrGraph[][]
        // .capacity() is used instead of .size() to calculate actual memory allocated
        bytes += sizeof(prrGraph)
            + prrGraph.capacity() * sizeof(decltype(prrGraph)::value_type);
        for (const auto& inner : prrGraph) {
            bytes += inner.capacity() * sizeof(std::remove_cvref_t<decltype(inner)>::value_type);
        }
        // Total bytes of contrib[][]
        bytes += sizeof(contrib)
            + contrib.capacity() * sizeof(decltype(contrib)::value_type);
        for (const auto& inner: contrib) {
            bytes += inner.capacity() * sizeof(std::remove_cvref_t<decltype(inner)>::value_type);
        }
        // Total bytes of totalGain[]
        bytes += sizeof(totalGain) 
            + totalGain.capacity() * sizeof(decltype(totalGain)::value_type);

        return bytes; 
    }

    /*
    * Returns the sum of the number of nodes in each PRR-sketch
    * i.e. sum |R_i| for i = 1 to |R|
    */
    [[nodiscard]] std::size_t nTotalNodes() const {
        auto res = std::size_t{ 0 }; 
        for (const auto& inner : prrGraph) {
            res += inner.size(); 
        }
        return res; 
    }

    /*
    * Dumps info on the PRRGraphCollection object
    * Returns a string of multiple lines, an '\n' appended at the tail
    */
    [[nodiscard]] std::string dump() const {
        // The default locale
        auto loc = std::locale("");
        auto info = format("n = {}\n", n); 
        // Dumps total number of nodes 
        auto nNodes = nTotalNodes(); 
        info += format(loc, "Total number of nodes = {}, {:.3f} per PRR-sketch in average\n", 
            nNodes, 1.0 * nNodes / prrGraph.size());
        // Dumps memory used
        auto bytes = totalBytesUsed();
        info += format(loc, "Memory used = {} bytes", bytes);
        if (bytes >= 1024) {
            static const char* units[3] = { "KibiBytes", "MebiBytes", "GibiBytes" };
            double value = bytes / 1024.0;
            int unitId = 0;

            for (; unitId < 2 && value >= 1024.0; ++unitId, value /= 1024.0) {} 
            info += format(" = {:.3f} {}", value, units[unitId]);
        }
        info += '\n';

        return info; 
    }
};

struct IMMResult {
    std::vector<std::size_t>    boostedNodes; 
    double                      totalGain; 
};

/*
* Returns the result of PR_IMM algorithm with given seed set and arguments
* ** FOR MONOTONE & SUB-MODULAR CASES **
*/
IMMResult PR_IMM(IMMGraph& graph, const SeedSet& seeds, AlgorithmArguments args); 

#endif //DAWNSEEKER_IMM_H
#pragma clang diagnostic pop