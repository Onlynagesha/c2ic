#ifndef DAWNSEEKER_IMMBASIC_H
#define DAWNSEEKER_IMMBASIC_H

#include "global.h"
#include "Graph.h"
#include "utils.h"
#include <algorithm>
#include <array>
#include <cassert>
#include <compare>
#include <filesystem>
#include <fstream>
#include <random>
#include <vector>

struct ReturnsValueTag {};
inline auto returnsValue = ReturnsValueTag{};

enum class NodeState {
    None = 0,       // Neither positive nor negative
    CaPlus = 1,     // Ca+: Boosted node WITH positive message
                    //      which propagates WITH higher probability
    Ca = 2,         // Ca:  Non-boosted node WITH positive message
    Cr = 3,         // Cr:  Non-boosted node WITH negative message
    CrMinus = 4     // Cr-: Boosted node WITH negative message
                    //      which propagates 'neutralized' negative message instead
};

enum class LinkState {
    NotSampleYet = 0,   // Not sampled initially
    Blocked = 1,        // Blocked link: unable to propagate any message
    Active = 2,         // Active link:  message propagates WITH probability p
    Boosted = 3         // Boosted link: for a boosted link u -> v, 
                        //  if u receives boosted positive message (Ca+), 
                        //  it propagates WITH probability pBoost >= p.
                        //  Others (Ca, Cr, Cr-) propagate WITH probability p as usual
};

/*
* Let f(S) = the total gain WITH S = the set of boosted nodes
*            as the expected number of extra nodes that get positive message due to S
*            compared to that WITH no boosted nodes
*     g(S) = the total gain WITH S = the set of boosted nodes
*            as the expected number of nodes prevented from negative message due to S
*            compared to that WITH no boosted nodes
* Then the objective function h(S) = lambda * f(S) + (1-lambda) * g(S)
* Gain of each node state (WITH parameter lambda):
*   None:   0
*   Ca+:    lambda
*   Ca:     lambda
*   Cr:     -(1-lambda)
*   Cr-:    0
*/
inline std::array<double, 5> nodeStateGain;

/*
* Priority of the node states, higher is better
* e.g. for the case Ca+ > Cr- > Cr > Ca,
*       priority[Ca+]  = 3,
*       priority[Cr-]  = 2,
*       priority[Cr]   = 1,
*       priority[Ca]   = 0,
*       priority[None] = -1 (None is always considered the lowest)
*/
inline std::array<int, 5> nodeStatePriority;

/*
* Compares two node states by priority
* A > B means A has higher priority than B
* WARNING ON (POSSIBLE) COMPILER BUG:
*  Implicit comparison operators (<, <=, etc.) may fail to work correctly
*   in GCC 11.2 and/or other compilers
*  Use explicit operator <=> instead. e.g. (A <=> B) == std::strong_ordering::greater
*/
inline std::strong_ordering operator <=> (NodeState A, NodeState B) {
    return nodeStatePriority[static_cast<int>(A)] <=> nodeStatePriority[static_cast<int>(B)];
}

/*
 * Compares two node states by priority
 * compare(A, B)  > 0 means A has higher priority than B
 * compare(A, B) == 0:      A and B are the same
 * compare(A, B)  < 0:      A has lower priority than B
 */
inline int compare(const std::array<int, 5>& priority, NodeState A, NodeState B) {
    return priority[static_cast<int>(A)] - priority[static_cast<int>(B)];
}

inline int compare(NodeState A, NodeState B) {
    return compare(nodeStatePriority, A, B);
}

/*
 * Checks equality of two node states
 * A more efficient overload since priority[] is not cared about.
 */
inline bool operator == (NodeState A, NodeState B) {
    return static_cast<int>(A) == static_cast<int>(B);
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
* Sets the gain of each state WITH given parameter lambda
*/
inline void setNodeStateGain(double lambda) {
    // Default state: 0.0
    nodeStateGain[static_cast<int>(NodeState::None)] = 0.0;
    // Gain WITH positive state: lambda
    nodeStateGain[static_cast<int>(NodeState::CaPlus)] = lambda;
    nodeStateGain[static_cast<int>(NodeState::Ca)] = lambda;
    // Gain WITH negative state: - (1.0 - lambda)
    nodeStateGain[static_cast<int>(NodeState::Cr)] = lambda - 1.0;
    // Gain WITH neutralized negative state: 0
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
inline std::array<int, 5> setNodeStatePriority(ReturnsValueTag, int caPlus, int ca, int cr, int crMinus) {
    // Checks the arguments to ensure they cover {0, 1, 2, 3}
    assert(((1 << caPlus) | (1 << ca) | (1 << cr) | (1 << crMinus)) == 0b1111);

    auto dest = std::array<int, 5>{};
    dest[static_cast<int>(NodeState::None)] = -1;
    dest[static_cast<int>(NodeState::CaPlus)] = caPlus;
    dest[static_cast<int>(NodeState::Ca)] = ca;
    dest[static_cast<int>(NodeState::Cr)] = cr;
    dest[static_cast<int>(NodeState::CrMinus)] = crMinus;

    return dest;
}

inline void setNodeStatePriority(int caPlus, int ca, int cr, int crMinus) {
    nodeStatePriority = setNodeStatePriority(returnsValue, caPlus, ca, cr, crMinus);
}

// Properties of the node priority
struct NodePriorityProperty {
    bool monotonic;     // Is monotonicity satisfied
    bool submodular;    // Is sub-modularity satisfied

    static NodePriorityProperty of(int caPlus, int ca, int cr, int crMinus) noexcept{
        return of(setNodeStatePriority(returnsValue, caPlus, ca, cr, crMinus));
    }

    static NodePriorityProperty of(const std::array<int, 5>& priority) noexcept {
        auto oldPriority = nodeStatePriority;
        // Temporarily sets to current priority
        nodeStatePriority = priority;
        // Gets current
        auto res = current();
        // and then restores
        nodeStatePriority = oldPriority;
        return res;
    }

    static NodePriorityProperty current() {
        using enum NodeState;
        static auto greater = std::strong_ordering::greater;

        auto res = NodePriorityProperty{.monotonic = true, .submodular = false};
        // Non-monotonic cases
        if (      (Ca <=> Cr) == greater          && (Cr <=> CaPlus) == greater
               || (Ca <=> CrMinus) == greater     && (CrMinus <=> CaPlus) == greater
               || (CrMinus <=> CaPlus) == greater && (CaPlus <=> Cr) == greater
               || (CrMinus <=> Ca) == greater     && (Ca <=> Cr) == greater) {
            res.monotonic = false;
        }
        // Sub-modular cases
        static auto submodularCases = {
                setNodeStatePriority(returnsValue, 3, 2, 0, 1), // Ca+ > Ca  > Cr- > Cr
                setNodeStatePriority(returnsValue, 3, 0, 1, 2), // Ca+ > Cr- > Cr  > Ca
                setNodeStatePriority(returnsValue, 1, 0, 2, 3)  // Cr- > Cr  > Ca+ > Ca
        };
        for (const auto& c: submodularCases) {
            if (nodeStatePriority == c) {
                res.submodular = true;
            }
        }

        return res;
    }

    // Checks with given token sequence (e.g. "M - nS"). Tokens are case-insensitive
    // "M":  expected to be monotonic
    // "nM":                non-monotonic
    // "S":  expected to be submodular
    // "nS":                non-submodular
    bool satisfies(const CaseInsensitiveString& str) {
        // Tokens are split by either of the following delimiters
        static const char* delims = " ,-;";
        // {token, which value to check, expected value}
        static auto checkItems = {
                std::tuple{"M",  &NodePriorityProperty::monotonic,  true},
                std::tuple{"mM", &NodePriorityProperty::monotonic,  false},
                std::tuple{"S",  &NodePriorityProperty::submodular, true},
                std::tuple{"nS", &NodePriorityProperty::submodular, false}
        };

        for (std::size_t next, pos = str.find_first_not_of(delims);
             pos != str.length() && pos != std::string::npos;
             pos = str.find_first_not_of(delims, next)) {
            next = str.find_first_of(delims, pos);
            auto token = str.substr(pos, (next == std::string::npos ? str.length() : next) - pos);

            // If current token matches any item
            bool matched = false;
            for (auto [token2b, which, expected]: checkItems) {
                if (token == token2b) {
                    matched = true;
                    if (this->*which != expected) {
                        return false;
                    }
                }
            }
            // If no match, then the token input is invalid
            if (!matched) {
                std::cerr << "WARNING on NodePriorityProperty::satisfies: token '" << toString(token)
                          << "' is not recognized. "
                             "Use one of 'M', 'nM', 'S', 'nS' instead." << std::endl;
            }
        }
        // All satisfied
        return true;
    }

    // Dumps as string
    [[nodiscard]] std::string dump() const {
        return format("{}monotonic & {}submodular ({} - {})",
                      monotonic ? "" : "non-",
                      submodular ? "" : "non-",
                      monotonic ? "M" : "nM",
                      submodular ? "S" : "nS");
    }
};

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
        // Initialize each bitset WITH the max index respectively
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

    // Total number of seeds, including Sa and Sr
    [[nodiscard]] std::size_t size() const {
        return _Sa.size() + _Sr.size();
    }

#pragma clang diagnostic push
#pragma ide diagnostic ignored "bugprone-sizeof-container"
    [[nodiscard]] std::size_t totalBytesUsed() const {
        std::size_t res = sizeof(_Sa) + sizeof(_Sr) + sizeof(_bitsetA) + sizeof(_bitsetR);
        res += _Sa.capacity() * sizeof(decltype(_Sa)::value_type);
        res += _Sr.capacity() * sizeof(decltype(_Sr)::value_type);
        // The two bitsets
        res += _bitsetA.capacity() / 8;
        res += _bitsetR.capacity() / 8;

        return res;
    }
#pragma clang diagnostic pop
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
        constexpr static double A = quickPow(0.5, B);
        // Fast generation of pseudo-random value in [0, 1)
        double r = (double)gen() * A;
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
    * On testing scenario, it ensures the same generation results WITH the same seed
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

// Node type of the whole graph for IMM algorithms
struct IMMNode: graph::BasicNode<std::size_t> {
    NodeState   state;
    int         dist;
    bool        boosted;

    IMMNode() = default;
    explicit IMMNode(std::size_t idx): graph::BasicNode<std::size_t>(idx) {} // NOLINT(cppcoreguidelines-pro-type-member-init)
};

// Graph type for IMM algorithms
using IMMGraph = graph::Graph<
        IMMNode,
        IMMLink,
        graph::IdentityIndexMap,
        graph::tags::EnablesFastAccess::Yes
        >;

/*
* Reads the graph.
* File format:
* The first line contains two integers V, E, denoting number of nodes and links
* Then E lines, each line contains 4 values u, v, p, pBoost
*   indicating a directed link u -> v WITH probabilities p and pBoost
*/
inline IMMGraph readGraph(std::istream& in) {
    auto graph = IMMGraph(graph::tags::reservesLater);

    std::size_t V, E;
    in >> V >> E;
    graph.reserve({{"nodes", V}, {"links", E}});

    for (std::size_t i = 0; i < V; i++) {
        graph.fastAddNode(IMMNode(i));
    }

    std::size_t from, to;
    double p, pBoost;
    for (; in >> from >> to >> p >> pBoost; ) {
        assert(from < V && to < V);
        graph.fastAddLink(IMMLink(from, to, p, pBoost));
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

struct SimResultItem {
    double positiveGain;    // positive = sum of all the gain(v) > 0
    double negativeGain;    // negative = sum of all the gain(v) < 0
    double totalGain;       // total = positive + negative

    void add(double value) {
        totalGain += value;
        if (value > 0.0) {
            positiveGain += value;
        } else {
            negativeGain += value;
        }
    }

    SimResultItem& operator += (const SimResultItem& rhs) {
        positiveGain += rhs.positiveGain;
        negativeGain += rhs.negativeGain;
        totalGain += rhs.totalGain;
        return *this;
    }

    SimResultItem& operator /= (std::size_t n) {
        positiveGain /= (double) n;
        negativeGain /= (double) n;
        totalGain /= (double) n;
        return *this;
    }

    SimResultItem operator - (const SimResultItem& rhs) const {
        return SimResultItem{
            .positiveGain = this->positiveGain - rhs.positiveGain,
            .negativeGain = this->negativeGain - rhs.negativeGain,
            .totalGain = this->totalGain - rhs.totalGain
        };
    }
};

inline std::string toString(const SimResultItem& item) noexcept {
    return format(
            "{{.totalGain = {:.2f}, .positiveGain = {:.2f}, .negativeGain = {:.2f}}}",
            item.totalGain, item.positiveGain, item.negativeGain);
}

struct SimResult {
    SimResultItem withBoosted;
    SimResultItem withoutBoosted;
    SimResultItem diff;     // diff = without boosted - without boosted
};

// Converts to a multi-line string
inline std::string toString(const SimResult& res) noexcept {
    return format(
            "{{\n"
            "       .withBoosted = {0}\n"
            "    .withoutBoosted = {1}\n"
            "              .diff = {2}\n}}",
            res.withBoosted, res.withoutBoosted, res.diff);
}

/*
 * Simulates message propagation with given boosted nodes
 * Each simulation sums up the gain of all the nodes,
 *  Ca and Ca+ counted as lambda, Cr as lambda - 1, Cr- and None as 0.
 * Returns:
 *  (1) Results with boosted nodes
 *  (2) Results without boosted nodes
 *  (3) Difference of the two, with - without
 */
SimResult simulate(IMMGraph& graph, const SeedSet& seeds,
                   const std::vector<std::size_t>& boostedNodes, std::size_t simTimes);

#endif //DAWNSEEKER_IMMBASIC_H