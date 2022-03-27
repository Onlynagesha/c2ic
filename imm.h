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
struct PRRGraphCollection {
    // Tag indicating the collection type shall be initialized later
    struct InitLater {};
    static inline auto initLater = InitLater{};

    struct Node {
        std::size_t index;
        NodeState   centerStateTo;
    };

    // Simplified PRR-sketch type
    // centerState: original state of the center node if no boosted nodes impose influence
    // items: nodes in the PRR-sketch
    //  for [v, centerStateTo] in items:
    //   centerStateTo = which state the center node will become if node v is set boosted
    struct SimplifiedPRRGraph {
        NodeState           centerState;
        std::vector<Node>   items;
    };

    // Number of nodes in the graph, i.e. |V|
    std::size_t                         n{};
    // Seed set
    SeedSet                             seeds;
    // prrGraph[i] = the i-th PRR-sketch
    std::vector<SimplifiedPRRGraph>     prrGraph;
    // contrib[v] = All the PRR-sketches where boosted node v changes the center node's state
    // for [i, centerStateTo] in contrib[v]:
    //  centerStateTo = which state the center node in the i-th PRR-sketch will become
    //                  if node v is set boosted
    std::vector<std::vector<Node>>      contrib;
    // totalGain[v] = total gain of the node v
    std::vector<double>                 totalGain;

    // Default constructor is deleted to force initialization
    PRRGraphCollection() = delete;
    // Delays initialization WITH the tag type
    explicit PRRGraphCollection(InitLater) {}
    // Initializes WITH |V| and the seed set
    explicit PRRGraphCollection(std::size_t n, SeedSet seeds) :
    n(n), seeds(std::move(seeds)), contrib(n), totalGain(n, 0.0) {}

    // Initializes WITH |V| and the seed set
    void init(std::size_t _n, SeedSet _seeds) {
        this->n = _n;
        this->seeds = std::move(_seeds);
        contrib.clear();
        contrib.resize(_n);
        totalGain.resize(_n, 0.0);
    }

    // Adds a PRR-sketch
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
            auto v = node.index();
            // Boosted node v, and the state changes the center node will change to
            prrList.push_back(Node{ .index = v, .centerStateTo = node.centerStateTo });
            // Boosted node v has influence on current PRR-sketch
            contrib[v].push_back(Node{.index = prrListId, .centerStateTo = node.centerStateTo });
            totalGain[v] += nodeGain;
        }

        // Empty PRR-sketch (if no node makes positive gain) is also skipped to same memory usage
        if (!prrList.empty()) {
            prrGraph.push_back(SimplifiedPRRGraph{
                .centerState = G.centerState,
                .items = std::move(prrList)
            });
        }
    }

private:
    // We use a change record list to make changes to and restore the PRR-sketch list
    // for [i, oldCenterState] in changeRecords (in order),
    //  The i-th PRR-sketch has centerState = oldCenterState before change
    // Since count of changes may be much smaller than the number of PRR-sketches stored,
    //  a change record is possibly more efficient than a full copy of all the states of PRR-sketches
    struct StateChangeRecord {
        std::size_t index;
        NodeState   oldCenterState;
    };
    std::vector<StateChangeRecord>  changeRecords;

    // Changes the centerState of the specified PRR-sketch and adds the change record
    void changeCenterState (std::size_t prrIndex, NodeState to) {
        changeRecords.push_back(StateChangeRecord{
                .index = prrIndex,
                .oldCenterState = this->prrGraph[prrIndex].centerState
        });
        prrGraph[prrIndex].centerState = to;
    };

    // Restores the centerState of all PRR-sketches
    // and clears the change record list after restoration
    void restoreCenterState () {
        // Perform restoration in reversed order
        for (const auto& rec: changeRecords | vs::reverse) {
            prrGraph[rec.index].centerState = rec.oldCenterState;
        }
        LOG_DEBUG(format("Number of PRR-sketches stored = {} vs. Size of PRR-sketch change record list: {}",
                        prrGraph.size(), changeRecords.size()));
        // Clears after all changes are restored
        changeRecords.clear();
    };

    // Helper non-const function of greedy selection
    template <class OutIter>
    requires std::output_iterator<OutIter, std::size_t>
             || std::same_as<OutIter, std::nullptr_t>
    double _select(std::size_t k, OutIter iter) {
        double res = 0.0;
        // Makes a copy of the totalGain[] to update values during selection
        auto totalGainCopy = totalGain;
        // The seeds shall not be selected
        for (auto a: seeds.Sa()) {
            totalGainCopy[a] = halfMin<double>;
        }
        for (auto r: seeds.Sr()) {
            totalGainCopy[r] = halfMin<double>;
        }

        for (std::size_t i = 0; i < k; i++) {
            // v = The node chosen in this turn, WITH the highest total gain
            std::size_t v = rs::max_element(totalGainCopy) - totalGainCopy.begin();
            // Output is only enabled when iter != nullptr
            if constexpr (!std::is_same_v<OutIter, std::nullptr_t>) {
                *iter++ = v;
            }
            res += totalGainCopy[v];
            LOG_DEBUG(format("Selected node #{}: index = {}, result += {:.2f}", i + 1, v, totalGainCopy[v]));
            // Sets the total gain of selected node as -inf
            //  so that it can never be selected again
            totalGainCopy[v] = halfMin<double>;

            // Impose influence to all the PRR-sketches by node v
            for (auto [prrId, centerStateTo] : contrib[v]) {
                // Attempts to update the center state of current PRR-sketch...
                auto cmp = centerStateTo <=> prrGraph[prrId].centerState;
                // ...only if the update makes higher priority.
                // (otherwise, the PRR-sketch may have been updated by other selected boosted nodes)
                if (cmp != std::strong_ordering::greater) {
                    continue;
                }
                double curGain = gain(centerStateTo) - gain(prrGraph[prrId].centerState);
                // After the center state of the prrId-th PRR-sketch changed from C0 to C1,
                //  all other nodes that may change the same PRR-sketch will make lower gain,
                //  from (C2 - C0) to (C2 - C1), diff = C1 - C0
                for (auto [j, s] : prrGraph[prrId].items) {
                    totalGainCopy[j] -= curGain;
                }
                // Updates state of the prrId-th PRR-sketch
                changeCenterState(prrId, centerStateTo);
            }
        }

        restoreCenterState();
        return res;
    }

public:
    // Selects k boosted nodes WITH greedy algorithm
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
        // The changeRecord list may be changed, so a non-const helper is needed
        //  though nothing is changed finally (all changes will be restored)
        return const_cast<PRRGraphCollection*>(this)->_select(k, iter);
    }

#pragma clang diagnostic push
#pragma ide diagnostic ignored "bugprone-sizeof-container"
    /*
    * Returns an estimation of total bytes used
    */
    [[nodiscard]] std::size_t totalBytesUsed() const {
        // n and seeds
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

        // Total bytes of changeRecord[]
        bytes += sizeof(changeRecords)
            + changeRecords.capacity() * sizeof(decltype(changeRecords)::value_type);

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
        auto info = format("Graph size |V| = {}\nNumber of PRR-sketches stored = {}\n", n, prrGraph.size());

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

            for (; unitId < 2 && value >= 1024.0; ++unitId) {
                value /= 1024.0;
            }
            info += format(" = {:.3f} {}", value, units[unitId]);
        }
        return info; 
    }
};

struct IMMResult {
    std::vector<std::size_t>    boostedNodes; 
    double                      totalGain; 
};

/*
* Returns the result of PR_IMM algorithm WITH given seed set and arguments
* ** FOR MONOTONE & SUB-MODULAR CASES **
*/
IMMResult PR_IMM(IMMGraph& graph, const SeedSet& seeds, AlgorithmArguments args); 

#endif //DAWNSEEKER_IMM_H