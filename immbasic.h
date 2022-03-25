#ifndef DAWNSEEKER_IMMBASIC_H
#define DAWNSEEKER_IMMBASIC_H

#include "global.h"
#include "Graph.h"
#include <algorithm>
#include <cassert>
#include <compare>
#include <filesystem>
#include <fstream>
#include <random>
#include <vector>

enum class NodeState {
    None = 0,       // Neither positive nor negative
    CaPlus = 1,     // Ca+: Boosted node with positive message
                    //      which propagates with higher probability
    Ca = 2,         // Ca:  Non-boosted node with positive message
    Cr = 3,         // Cr:  Non-boosted node with negative message
    CrMinus = 4     // Cr-: Boosted node with negative message
                    //      which propagates 'neutralized' negative message instead
};

enum class LinkState {
    NotSampleYet = 0,   // Not sampled initially
    Blocked = 1,        // Blocked link: unable to propagate any message
    Active = 2,         // Active link:  message propagates with probability p
    Boosted = 3         // Boosted link: for a boosted link u -> v, 
                        //  if u receives boosted positive message (Ca+), 
                        //  it propagates with probability pBoost >= p.
                        //  Others (Ca, Cr, Cr-) propagate with probability p as usual 
};

/*
* Let f(S) = the total gain with S = the set of boosted nodes
*            as the expected number of extra nodes that get positive message due to S
*            compared to that with no boosted nodes
*     g(S) = the total gain with S = the set of boosted nodes
*            as the expected number of nodes prevented from negative message due to S
*            compared to that with no boosted nodes
* Then the objective function h(S) = lambda * f(S) + (1-lambda) * g(S)
* Gain of each node state (with parameter lambda):
*   None:   0
*   Ca+:    lambda
*   Ca:     lambda
*   Cr:     -(1-lambda)
*   Cr-:    0
*/
inline double   nodeStateGain[5];

/*
* Priority of the node states, higher is better
* e.g. for the case Ca+ > Cr- > Cr > Ca, 
*       priority[Ca+]  = 3,
*       priority[Cr-]  = 2,
*       priority[Cr]   = 1,
*       priority[Ca]   = 0,
*       priority[None] = -1 (None is always considered the lowest)
*/
inline int      nodeStatePriority[5];

/*
* Compares two node states by priority
* A > B means A has higher priority than B
*/
inline std::strong_ordering operator <=> (NodeState A, NodeState B) {
    return nodeStatePriority[static_cast<int>(A)] <=> nodeStatePriority[static_cast<int>(B)];
}

/*
* Compares two link states as order Boosted > Active > Blocked
*/
inline std::strong_ordering operator <=> (LinkState A, LinkState B) {
    return static_cast<int>(A) <=> static_cast<int>(B);
}

/*
* Gets the gain of certain state
*/
inline double gain(NodeState state) {
    return nodeStateGain[static_cast<int>(state)];
}

/*
* Gets whether the state is positive (Ca+ or Ca)
*/
inline bool isPositive(NodeState state) {
    return state == NodeState::CaPlus || state == NodeState::Ca;
}

/*
* Gets whether the state is negative (Cr- or Cr)
*/
inline bool isNegative(NodeState state) {
    return state == NodeState::Cr || state == NodeState::CrMinus;
}

/*
* Sets the gain of each state with given parameter lambda
*/
inline void setNodeStateGain(double lambda) {
    // Default state: 0.0
    nodeStateGain[static_cast<int>(NodeState::None)] = 0.0;
    // Gain with positive state: lambda
    nodeStateGain[static_cast<int>(NodeState::CaPlus)] = lambda;
    nodeStateGain[static_cast<int>(NodeState::Ca)] = lambda;
    // Gain with negative state: - (1.0 - lambda)
    nodeStateGain[static_cast<int>(NodeState::Cr)] = lambda - 1.0;
    // Gain with neutralized negative state: 0
    nodeStateGain[static_cast<int>(NodeState::CrMinus)] = 0.0;
}

/*
* Sets the priority of each state
* e.g. For the case Ca+ > Cr- > Cr > Ca,
*       caPlus  = 3 (highest)
*       ca      = 0 (lowest)
*       cr      = 1 (2nd lowest)
*       crMinus = 2 (2nd highest)
*/
inline void setNodeStatePriority(int caPlus, int ca, int cr, int crMinus) {
    // Checks the arguments to ensure they cover {0, 1, 2, 3}
    assert(((1 << caPlus) | (1 << ca) | (1 << cr) | (1 << crMinus)) == 0b1111);

    nodeStatePriority[static_cast<int>(NodeState::None)] = -1;
    nodeStatePriority[static_cast<int>(NodeState::CaPlus)] = caPlus;
    nodeStatePriority[static_cast<int>(NodeState::Ca)] = ca;
    nodeStatePriority[static_cast<int>(NodeState::Cr)] = cr;
    nodeStatePriority[static_cast<int>(NodeState::CrMinus)] = crMinus;
}

/*
* Converts a node state to string
*/
constexpr const char* toString(NodeState t) {
    switch (t) {
    case NodeState::None:
        return "None";
    case NodeState::CaPlus:
        return "Ca+";
    case NodeState::Ca:
        return "Ca";
    case NodeState::Cr:
        return "Cr";
    case NodeState::CrMinus:
        return "Cr-";
    default:
        return "(ERROR)";
    }
}

/*
* Converts a link state to string
*/
constexpr const char* toString(LinkState t) {
    switch (t) {
    case LinkState::NotSampleYet:
        return "Unsampled";
    case LinkState::Blocked:
        return "Blocked";
    case LinkState::Active:
        return "Active";
    case LinkState::Boosted:
        return "Boosted";
    default:
        return "(ERROR)";
    }
}

/*
* Formatter of node state or link state.
* for std::format or fmt::format
*/
template <class State> 
    requires std::same_as<State, NodeState> || std::same_as<State, LinkState>
struct FORMAT_NAMESPACE::formatter<State, char>:
    FORMAT_NAMESPACE::formatter<const char*, char> {

    template<class FormatContext>
    auto format(State t, FormatContext& fc) {
        return FORMAT_NAMESPACE::formatter<const char*, char>::format(toString(t), fc);
    }
};

/*
* Seed set. 
* Sa = Seed set of positive message (Ca)
* Sr = Seed set of negative message (Cr)
*/
class SeedSet {
    std::vector<std::size_t>    _Sa;
    std::vector<std::size_t>    _Sr;
    // _bitsetA[v] = whether node v is in Sa
    std::vector<bool>           _bitsetA;
    // _bitsetR[v] = whether node v is in Sr
    std::vector<bool>           _bitsetR;

public:
    SeedSet() = default;
    SeedSet(std::vector<std::size_t> Sa, std::vector<std::size_t> Sr) {
        init(std::move(Sa), std::move(Sr)); 
    }

    void init(std::vector<std::size_t> Sa, std::vector<std::size_t> Sr) {
        this->_Sa = std::move(Sa);
        this->_Sr = std::move(Sr);
        // Sort respectively for binary search (if required)
        rs::sort(this->_Sa);
        rs::sort(this->_Sr);
        // Initialize each bitset with the max index respectively
        _bitsetA.resize(_Sa.back() + 1, false);
        _bitsetR.resize(_Sr.back() + 1, false);

        for (auto a : _Sa) {
            _bitsetA[a] = true; 
        }
        for (auto r : _Sr) {
            _bitsetR[r] = true; 
        }
    }

    // Whether the specified node is in Sa
    [[nodiscard]] bool containsInSa(std::size_t index) const {
        return index < _bitsetA.size() && _bitsetA[index]; 
    }

    // Whether the specified node is in Sr
    [[nodiscard]] bool containsInSr(std::size_t index) const {
        return index < _bitsetR.size() && _bitsetR[index];
    }

    // Whether the specified node is in either Sa or Sr
    [[nodiscard]] bool contains(std::size_t index) const {
        return containsInSa(index) || containsInSr(index);
    }

    [[nodiscard]] const auto& Sa() const {
        return _Sa;
    }
    [[nodiscard]] const auto& Sr() const {
        return _Sr;
    }
};

/*
* p = probability that certain non-boosted positive message, 
*       or negative message propagates along this link
* pBoost = probability that boosted positive message propagates along this link
*       (ensures that pBoost >= p)
*/
struct IMMLink: graph::BasicLink<std::size_t> {
private:
    // Internal pseudo-random generator
    // The seed of generator is set related to system clock by default.
    // If required that the sample sequence is fixed (on debugging for example),
    //  set the seed via .setSeed()
    static inline auto gen = createMT19937Generator();

    // Each random-generated state is assigned a timestamp. 
    // After refreshing the state of one link, its timestamp is set equal to globalTimeStamp
    //  which indicates its state is up-to-date.
    // On .getState() call, first check whether the state is up-to-date.
    // If not, refresh first and then return.
    // To refresh states globally for all the links, simply increment globalTimeStamp by 1
    //  so that O(1) lazy global refreshing is performed. 
    static inline auto globalTimestamp = unsigned(0);

    // Timestamp of the state
    unsigned    stateTimestamp; 
    // State of the link
    LinkState	state;

public:
    double		p;
    double		pBoost; 

    IMMLink() = default;

    IMMLink(std::size_t from, std::size_t to, double p, double pBoost) :BasicLink(from, to),
        // On construction, sets its timestamp as outdated
        //  to trigger refreshing on .getState() call.
        stateTimestamp(globalTimestamp - 1), state(LinkState::NotSampleYet), 
        p(p), pBoost(pBoost) {}

    /*
    * Gets the state of the link
    * If never sampled before, or the sample result is outdated, do sample once; 
    * otherwise, returns the last sample result
    */
    [[nodiscard]] LinkState getState() const {
        if (stateTimestamp != globalTimestamp) {
            const_cast<IMMLink*>(this)->refreshState();
        }
        return state; 
    }

    /*
    * Gets the state of the link no matter whether it is NotSampleYet or outdated
    * This version is recommended when it's ensured the link is sampled up-to-date already
    */
    [[nodiscard]] LinkState forceGetState() const {
        return state; 
    }

    /*
    * Refreshes the state of this link, and sets its timestamp up-to-date.
    * With p probability:				Active 
    * With (pBoost - p) probability:	Boosted
    * With (1 - pBoost) probability:	Blocked
    */
    void refreshState() {
        // A = 2 ^ (-B) where B = number of bits of each generated unsigned integer
        constexpr int B = (int) decltype(gen)::word_size;
        constexpr static double A = std::ldexp(1.0, -B);
        // Fast generation of pseudo-random value in [0, 1)
        double r = gen() * A;
        // [0, p): Active
        if (r < p) {
            state = LinkState::Active;
        }
        // [p, pBoost): Boosted
        else if (r < pBoost) {
            state = LinkState::Boosted;
        }
        // [pBoost, 1): Blocked
        else {
            state = LinkState::Blocked;
        }
        // Set the timestamp up-to-date
        stateTimestamp = globalTimestamp;
    }

    /*
    * Sets the seed of the internal pseudo-random generator
    * On testing scenario, it ensures the same generation results with the same seed
    * (The default seed is something related to system clock)
    */
    static void setSeed(unsigned seed) {
        gen.seed(seed);
    }

    /*
    * Refreshes all the link states in lazy method.
    * All the link states get outdated and need re-sampling on next .getState() call.
    */
    static void refreshAllStates(unsigned seed = 0) {
        if (seed != 0) {
            setSeed(seed);
        }
        globalTimestamp += 1;
    }
};

using IMMGraph = graph::Graph<
        graph::BasicNode<std::size_t>,
        IMMLink,
        graph::MinimalIndexIdentity,
        graph::ReserveStrategy::Nodes | graph::ReserveStrategy::Links
        >;

/*
* Reads the graph.
* File format:
* The first line contains two integers V, E, denoting number of nodes and links
* Then E lines, each line contains 4 values u, v, p, pBoost
*   indicating a directed link u -> v with probabilities p and pBoost
*/
inline IMMGraph readGraph(std::istream& in) {
    auto graph = IMMGraph(graph::tags::reservesLater);

    std::size_t V, E;
    in >> V >> E;
    graph.reserve({{"nodes", V}, {"links", E}});

    std::size_t from, to;
    double p, pBoost;

    for (; in >> from >> to >> p >> pBoost; ) {
        graph.emplaceNode(from);
        graph.emplaceNode(to);
        graph.emplaceLink(from, to, p, pBoost);
    }
    return graph;
}

inline IMMGraph readGraph(const fs::path& path) {
    auto fin = std::ifstream(path);
    return readGraph(fin);
}

/*
 * Reads the seed set.
 * File format:
 * The first line contains an integer Na, number of positive seeds.
 * The next line contains Na integers, indices of the positive seeds.
 * The third line contains an integer Nr, number of negative seeds.
 * The fourth line contains Nr integers, indices of the negative seeds
 */
inline SeedSet readSeedSet(std::istream& in) {
    std::size_t Na, Nr;
    auto Sa = std::vector<std::size_t>();
    auto Sr = std::vector<std::size_t>();

    in >> Na;
    for (std::size_t v, i = 0; i < Na; in >> v, Sa.push_back(v), i++);
    in >> Nr;
    for (std::size_t v, i = 0; i < Nr; in >> v, Sr.push_back(v), i++);

    return {std::move(Sa), std::move(Sr)};
}

inline SeedSet readSeedSet(const fs::path& path) {
    auto fin = std::ifstream(path);
    return readSeedSet(fin);
}

#endif //DAWNSEEKER_IMMBASIC_H