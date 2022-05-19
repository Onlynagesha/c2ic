#ifndef DAWNSEEKER_IMMBASIC_H
#define DAWNSEEKER_IMMBASIC_H

#include "global.h"
#include <algorithm>
#include <array>
#include <compare>
#include <iomanip>
#include <random>
#include <sstream>
#include <vector>

struct ReturnsValueTag {};
inline auto returnsValue = ReturnsValueTag{};

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
    NotSampledYet = 0,   // Not sampled initially
    Blocked = 1,        // Blocked link: unable to propagate any message
    Active = 2,         // Active link:  message propagates with probability p
    Boosted = 3         // Boosted link: for a boosted link u -> v, 
                        //  if u receives boosted positive message (Ca+), 
                        //  it propagates with probability pBoost >= p.
                        //  Others (Ca, Cr, Cr-) propagate with probability p as usual
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
        case LinkState::NotSampledYet:
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

using NodeStatePriorityArray = std::array<int, 5>;
using NodeStateGainArray = std::array<double, 5>;

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
inline NodeStateGainArray nodeStateGain;

/*
* Priority of the node states, higher is better
* e.g. for the case Ca+ > Cr- > Cr > Ca,
*       priority[Ca+]  = 3,
*       priority[Cr-]  = 2,
*       priority[Cr]   = 1,
*       priority[Ca]   = 0,
*       priority[None] = -1 (None is always considered the lowest)
*/
inline NodeStatePriorityArray nodeStatePriority;

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
inline int compare(const NodeStatePriorityArray& priority, NodeState A, NodeState B) {
    return priority[static_cast<int>(A)] - priority[static_cast<int>(B)];
}

/*
 * Compares with the global priority array
 */
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

/*!
 * @brief Sets the priority of each node state and returns the priority array as result.
 *
 * The priority values shall be a permutation of [0, 1, 2, 3]. Higher value refers to higher priority.
 *
 * e.g. For the case Ca+ > Cr- > Cr > Ca,
 *  - caPlus  = 3 (highest)
 *  - ca      = 0 (lowest)
 *  - cr      = 1 (2nd lowest)
 *  - crMinus = 2 (2nd highest)
 *
 * @param caPlus Priority of Ca+
 * @param ca Priority of Ca
 * @param cr Priority of Cr
 * @param crMinus Priority of Cr-
 * @return The priority array
 * @throw std::invalid_argument if the arguments do not satisfy the constraints above.
 */
inline NodeStatePriorityArray setNodeStatePriority(ReturnsValueTag, int caPlus, int ca, int cr, int crMinus) {
    // Checks the arguments to ensure they cover {0, 1, 2, 3}
    if (((1 << caPlus) | (1 << ca) | (1 << cr) | (1 << crMinus)) != 0b1111) {
        throw std::invalid_argument("Input priority values are not a permutation of [0, 1, 2, 3]");
    }

    auto dest = std::array<int, 5>{};
    dest[static_cast<int>(NodeState::None)] = -1;
    dest[static_cast<int>(NodeState::CaPlus)] = caPlus;
    dest[static_cast<int>(NodeState::Ca)] = ca;
    dest[static_cast<int>(NodeState::Cr)] = cr;
    dest[static_cast<int>(NodeState::CrMinus)] = crMinus;

    return dest;
}

/*!
 * @brief Sets the global node state priority.
 *
 * See setNodeStatePriority(caPlus, ca, cr, crMinus) for details.
 *
 * @param caPlus Priority of Ca+
 * @param ca Priority of Ca
 * @param cr Priority of Cr
 * @param crMinus Priority of Cr-
 * @throw std::invalid_argument if the arguments do not satisfy the constraints.
 */
inline void setNodeStatePriority(int caPlus, int ca, int cr, int crMinus) {
    nodeStatePriority = setNodeStatePriority(returnsValue, caPlus, ca, cr, crMinus);
}

/*!
 * @brief Sets the global node state priority with given array.
 * @param arr The priority array
 */
inline void setNodeStatePriority(const NodeStatePriorityArray& arr) {
    nodeStatePriority = arr;
}

/*!
 * @brief A class that contains properties of some node priority.
 *
 * Including:
 *   - The priority array
 *   - Whether the priority satisfies monotonicity and/or submodularity
 *     (i.e. the objective function is monotonic and/or submodular)
 */
struct NodePriorityProperty {
    NodeStatePriorityArray  array;          // The priority array
    bool                    monotonic;      // Is monotonicity satisfied
    bool                    submodular;     // Is sub-modularity satisfied

    /*!
     * @brief Gets the "upper bound" node state priority.
     *
     * Used in SA-IMM and SA-RG-IMM algorithms,
     *  acquiring the upper bound fraction of objective function by performing PR-IMM algorithm
     *  under this "upper bound" priority (which is monotonic & submodular).
     *
     * @return The NodePriorityProperty object of the upper bound.
     */
    static NodePriorityProperty upperBound() {
        return of("Ca+ Cr- Cr Ca");
    }

    /*!
     * @brief Gets a NodePriorityProperty object from given string.
     *
     * Format of input: permutation of `Ca+`, `Ca`, `Cr` and `Cr-`, separated by comma, space or `>`.
     * Input is case-insensitive.
     * e.g. `Ca+ > Ca > Cr > Cr-` or `cr+,cr,ca+,ca`.
     *
     * An exception is thrown for the following illegal cases:
     *   - Duplicated state token, e.g. `Ca+ Ca+ Ca Cr Cr-`
     *   - Unrecognized state token, e.g. `Cp+ Cp Cn- Cn`
     *   - Too few tokens, e.g. `Ca+ Ca Cr`
     *
     * @param str The input string with given input
     * @return The NodePriorityProperty object of given node state priority.
     * @throw std::invalid_argument if the input string is invalid. See above for details.
     */
    static NodePriorityProperty of(const utils::StringLike auto& str) {
        auto first = utils::toCString(str);
        auto last = first + utils::stringLength(str);

        // Sets in the order 3, 2, 1, 0.
        // arr[None] remains as -1.
        int nextPriority = 3;
        auto arr = NodeStatePriorityArray{};
        rs::fill(arr, -1);

        // Checks duplication and then assigns arr[s] = p
        auto trySet = [&](NodeState s_, int p) {
            int s = (int)s_;
            if (arr[s] != -1) {
                throw std::invalid_argument("Repeated node state: "s + toString(s_));
            }
            arr[s] = p;
        };

        // Sets each token in order.
        utils::cstr::splitByEither(first, last, " ,>", [&](const char* token, std::size_t tokenLength) {
            if (nextPriority < 0) {
                throw std::invalid_argument("Too much tokens (exactly 4 is required)");
            }

            if (utils::cstr::ci_strcmp(token, tokenLength, "Ca+", 3) == 0) {
                trySet(NodeState::CaPlus, nextPriority--);
            } else if (utils::cstr::ci_strcmp(token, tokenLength, "Ca", 2) == 0) {
                trySet(NodeState::Ca, nextPriority--);
            } else if (utils::cstr::ci_strcmp(token, tokenLength, "Cr", 2) == 0) {
                trySet(NodeState::Cr, nextPriority--);
            } else if (utils::cstr::ci_strcmp(token, tokenLength, "Cr-", 3) == 0) {
                trySet(NodeState::CrMinus, nextPriority--);
            } else {
                throw std::invalid_argument(
                        "Unrecognized token other than 'Ca+', 'Ca', 'Cr' or 'Cr-': "s
                        + std::string{token, tokenLength});
            }
        });

        // Checks whether tokens are too few.
        if (nextPriority != -1) {
            throw std::invalid_argument("Too few tokens (exactly 3 is required)");
        }

        return of(arr);
    }

    /*!
     * @brief Gets a NodePriorityProperty object from given node state priority values.
     *
     * See setNodeStatePriority(caPlus, ca, cr, crMinus) for details.
     *
     * @param caPlus Priority of Ca+
     * @param ca Priority of Ca
     * @param cr Priority of Cr
     * @param crMinus Priority of Cr-
     * @return The NodePriorityProperty object of given node state priority.
     * @throw std::invalid_argument if the arguments do not satisfy the constraints.
     */
    static NodePriorityProperty of(int caPlus, int ca, int cr, int crMinus) {
        return of(setNodeStatePriority(returnsValue, caPlus, ca, cr, crMinus));
    }

    /*!
     * @brief Gets a NodePriorityProperty object from given node state priority array.
     * @param priority The priority array
     * @return The NodePriorityProperty object of given node state priority.
     */
    static NodePriorityProperty of(const NodeStatePriorityArray& priority) {
        auto oldPriority = nodeStatePriority;
        // Temporarily sets to current priority
        nodeStatePriority = priority;
        // Gets current
        auto res = current();
        // and then restores
        nodeStatePriority = oldPriority;
        return res;
    }

    /*!
     * @brief Gets a NodePriorityProperty object of the current node state priority.
     * @return The NodePriorityProperty object of current node state priority.
     */
    static NodePriorityProperty current() {
        using enum NodeState;
        constexpr auto greater = std::strong_ordering::greater;

        auto res = NodePriorityProperty{
            .array = nodeStatePriority,
            .monotonic = true,
            .submodular = false
        };
        // Non-monotonic cases
        if (      (Ca <=> Cr) == greater          && (Cr <=> CaPlus) == greater         // (1) Ca  > Cr  > Ca+
               || (Ca <=> CrMinus) == greater     && (CrMinus <=> CaPlus) == greater    // (2) Ca  > Cr- > Ca+
               || (CrMinus <=> CaPlus) == greater && (CaPlus <=> Cr) == greater         // (3) Cr- > Ca+ > Cr
               || (CrMinus <=> Ca) == greater     && (Ca <=> Cr) == greater) {          // (4) Cr- > Ca  > Cr
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

    /*!
     * @brief Checks whether the node state satisfies the constraints given by input string.
     *
     * Input format: a sequence of tokens `M`, `nM`, `S` or `nS`
     * (for monotonic, non-monotonic, submodular, non-submodular resp.),
     * separated by space, comma, semicolon or hyphen.
     *
     * The input is case-insensitive.
     * An exception is thrown if some token is unrecognized.
     *
     * e.g.
     *   - `nM`: non-monotonic
     *   - `M - S`: both monotonic and submodular
     *   - `M, S; nM`: both monotonic, submodular and non-monotonic
     *     (whose result is always false, syntactically correct by semantically senseless)
     *
     * @param str The input string.
     * @return bool, whether all the constraints are satisfied.
     * @throw std::invalid_argument if the input is invalid. See above for details.
     */
    [[nodiscard]] bool satisfies(const ci_string& str) const {
        // Returns true if all the constraints are satisfied
        bool res = true;
        // Tokens are split by either of the following delimiters
        static const char* delims = " ,-;";
        // {token, which value to check, expected value}
        static auto checkItems = {
                std::tuple{"M",  &NodePriorityProperty::monotonic,  true},
                std::tuple{"nM", &NodePriorityProperty::monotonic,  false},
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
                        // Returns false if any constraint is violated.
                        res = false;
                    }
                }
            }
            // If no match, then the token input is invalid
            if (!matched) {
                throw std::invalid_argument(
                        "Unrecognized token other than 'M', 'nM', 'S' or 'nS': "s + utils::toString(token));
            }
        }
        return res;
    }

    /*!
     * @brief Dumps the information as string.
     * @return A multi-line string, without trailing new-line character.
     */
    [[nodiscard]] std::string dump() const {
        using enum NodeState;

        // Dumps priority array
        auto res = std::ostringstream("Priority values:\n");
        for (auto s: {None, CaPlus, Ca, Cr, CrMinus}) {
            res << "    " << std::left << std::setw(4) << toString(s) << " => " << array[(int)s] << '\n';
        }

        // Dumps comparison matrix
        res << "Comparison matrix of L <=> R:\nL\\R  Ca+  Ca  Cr Cr-\n";
        for (auto lhs: {CaPlus, Ca, Cr, CrMinus}) {
            res << std::left << std::setw(4) << toString(lhs);
            for (auto rhs: {CaPlus, Ca, Cr, CrMinus}) {
                int c = compare(array, lhs, rhs);
                res << std::right << std::setw(4) << (c > 0 ? '>' : c == 0 ? '=' : '<');
            }
            res.put('\n');
        }

        // Dumps property
        res << "Property: " << (monotonic ? "" : "non-") << "monotonic & "
            << (submodular ? "" : "non-") << "submodular ("
            << (monotonic ? "M" : "nM") << " - " << (submodular ? "S" : "nS") << ")";

        return res.str();
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

    // Total number of seeds, including Sa and Sr
    [[nodiscard]] std::size_t size() const {
        return _Sa.size() + _Sr.size();
    }

    [[nodiscard]] std::size_t totalBytesUsed() const {
        auto res = utils::totalBytesUsed(_Sa) + utils::totalBytesUsed(_Sr)
                + utils::totalBytesUsed(_bitsetA) + utils::totalBytesUsed(_bitsetR);

        return res;
    }
};

/*!
 * @brief Generates a random link state according to probabilities (p, pBoost).
 *
 *   - With p probability:              Active
 *   - With (pBoost - p) probability:   Boosted
 *   - With (1 - pBoost) probability:   Blocked
 *
 * @param p
 * @param pBoost
 * @return One of Active, Boosted or Blocked
 */
inline LinkState getRandomState(double p, double pBoost) {
    static auto gen = createMT19937Generator();
     //A = 2 ^ (-B) where B = number of bits of each generated unsigned integer
    constexpr static double A = quickPow(0.5, decltype(gen)::word_size);

    // Fast generation of pseudo-random value in [0, 1)
    double r = (double)gen() * A;
    // [0, p): Active
    if (r < p) {
        return LinkState::Active;
    }
    // [p, pBoost): Boosted
    else if (r < pBoost) {
        return LinkState::Boosted;
    }
    // [pBoost, 1): Blocked
    else {
        return LinkState::Blocked;
    }
}

#endif //DAWNSEEKER_IMMBASIC_H