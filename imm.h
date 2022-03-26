#ifndef DAWNSEEKER_IMM_H
#define DAWNSEEKER_IMM_H

#include "args.h"
#include "global.h"
#include "immbasic.h"
#include "Logger.h"
#include "PRRGraph.h"
#include <cmath>
#include <locale>
#include <numbers>
#include <vector> 

/*
* Collection of all PRR-sketches
*/
struct PRRGraphCollection { // NOLINT(cppcoreguidelines-pro-type-member-init)
    struct InitLater {};
    static inline auto initLater = InitLater{};

    struct Node {
        std::size_t index;
        NodeState   centerStateTo;
    };

    struct SimplifiedPRRGraph {
        NodeState           centerState;
        std::vector<Node>   items;
    };

    // Number of nodes
    std::size_t                         n{};
    // Seed set
    SeedSet                             seeds;
    // prrGraph[i] = the i-th PRR-sketch
    // Each sketch is simplified as a list of {v, g} pairs,
    //  indicating that node v makes gain = g in this PRR-sketch
    std::vector<SimplifiedPRRGraph>     prrGraph;
    // contrib[v] = list of indices of PRR-sketches
    //  where node v contributes non-zero gain
    std::vector<std::vector<Node>>      contrib;
    // totalGain[v] = total gain of the node v
    std::vector<double>                 totalGain;

    PRRGraphCollection() = delete;

    explicit PRRGraphCollection(InitLater) {}

    explicit PRRGraphCollection(std::size_t n, SeedSet seeds) :
    n(n), seeds(std::move(seeds)), contrib(n), totalGain(n, 0.0) {}

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wshadow"
    void init(std::size_t n, SeedSet seeds) {
        this->n = n;
        this->seeds = std::move(seeds);
        contrib.clear();
        contrib.resize(n);
        totalGain.resize(n, 0.0);
    }
#pragma clang diagnostic pop

    void add(const PRRGraph& G) {
        auto prrList = std::vector<Node>();
        // Index of the PRR-sketch to be added
        auto prrListId = prrGraph.size();

        for (const auto& node : G.nodes()) {
            double nodeGain = gain(node.centerStateTo) - gain(G.centerState);
            // Zero-gain nodes are skipped to save memory usage
            if (nodeGain <= 0.0) {
                continue;
            }
            auto nodeId = node.index();
            prrList.push_back(Node{ .index = nodeId, .centerStateTo = node.centerStateTo });
            contrib[nodeId].push_back(Node{.index = prrListId, .centerStateTo = node.centerStateTo });
            totalGain[nodeId] += nodeGain;
        }

        if (!prrList.empty()) {
            prrGraph.push_back(SimplifiedPRRGraph{
                .centerState = G.centerState,
                .items = std::move(prrList)
            });
        }
    }

private:
    struct StateChangeRecord {
        std::size_t index;
        NodeState   oldCenterState;
    };
    std::vector<StateChangeRecord>  changeRecords;

    void changeCenterState (std::size_t prrIndex, NodeState to) {
        changeRecords.push_back(StateChangeRecord{
                .index = prrIndex,
                .oldCenterState = this->prrGraph[prrIndex].centerState
        });
        prrGraph[prrIndex].centerState = to;
    };

    void restoreCenterState () {
        for (const auto& rec: changeRecords | vs::reverse) {
            prrGraph[rec.index].centerState = rec.oldCenterState;
        }
        LOG_INFO(format("Size of PRR-sketch changeRecords: {}", changeRecords.size()));
        changeRecords.clear();
    };

    template <class OutIter>
    requires std::output_iterator<OutIter, std::size_t>
             || std::same_as<OutIter, std::nullptr_t>
    double _select(std::size_t k, OutIter iter) {
        double res = 0.0;
        // Makes a copy of the totalGain[] to updateValues during selection
        auto totalGainCopy = totalGain;
        // The seeds shall not be selected
        for (auto a: seeds.Sa()) {
            totalGainCopy[a] = halfMin<double>;
        }
        for (auto r: seeds.Sr()) {
            totalGainCopy[r] = halfMin<double>;
        }

        for (std::size_t i = 0; i < k; i++) {
            // cur = The node chosen in this turn, with the highest total gain
            std::size_t cur = rs::max_element(totalGainCopy) - totalGainCopy.begin();
            // Output is only enabled when iter != nullptr
            if constexpr (!std::is_same_v<OutIter, std::nullptr_t>) {
                *iter++ = cur;
            }
            res += totalGainCopy[cur];
            LOG_INFO(format("Selected node #{}: index = {}, result += {:.2f}", i + 1, cur, totalGainCopy[cur]));
            // Sets the total gain of selected node as -inf
            //  so that it can never be selected again
            totalGainCopy[cur] = halfMin<double>;

            for (auto [prrId, centerStateTo] : contrib[cur]) {
                // Attempts to cover the center state of current PRR-sketch
                auto cmp = centerStateTo <=> prrGraph[prrId].centerState;
                if (cmp != std::strong_ordering::greater) {
                    continue;
                }
                double curGain = gain(centerStateTo) - gain(prrGraph[prrId].centerState);
                for (auto [j, s] : prrGraph[prrId].items) {
                    totalGainCopy[j] -= curGain;
                }
                changeCenterState(prrId, centerStateTo);
            }
        }

        restoreCenterState();
        return res;
    }

public:
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
    double select(std::size_t k, OutIter iter) const {
        double res = const_cast<PRRGraphCollection*>(this)->template _select(k, iter);
        return res;
    }

#pragma clang diagnostic push
#pragma ide diagnostic ignored "bugprone-sizeof-container"
    /*
    * Returns an estimation of bytes used
    */
    [[nodiscard]] std::size_t totalBytesUsed() const {
        auto bytes = sizeof(n) + seeds.totalBytesUsed();
        // Total bytes of prrGraph[][]
        // .capacity() is used instead of .size() to calculate actual memory allocated
        bytes += sizeof(prrGraph)
            + prrGraph.capacity() * sizeof(decltype(prrGraph)::value_type);
        for (const auto& inner : prrGraph) {
            bytes += inner.items.capacity() * sizeof(std::remove_cvref_t<decltype(inner.items)>::value_type);
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
#pragma clang diagnostic pop

    /*
    * Returns the sum of the number of nodes in each PRR-sketch
    * i.e. sum |R_i| for i = 1 to |R|
    */
    [[nodiscard]] std::size_t nTotalNodes() const {
        auto res = std::size_t{ 0 }; 
        for (const auto& inner : prrGraph) {
            res += inner.items.size();
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
            nNodes, 1.0 * (double)nNodes / (double)prrGraph.size());
        // Dumps memory used
        auto bytes = totalBytesUsed();
        info += format(loc, "Memory used = {} bytes", bytes);
        if (bytes >= 1024) {
            static const char* units[3] = { "KibiBytes", "MebiBytes", "GibiBytes" };
            double value = (double)bytes / 1024.0;
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