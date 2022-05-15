//
// Created by Onlynagesha on 2022/5/13.
//

#ifndef DAWNSEEKER_GRAPHBASIC_H
#define DAWNSEEKER_GRAPHBASIC_H

#include "immbasic.h"

using IMMNode = graph::BasicNode<std::size_t>;

struct IMMLink: graph::BasicLink<std::size_t> {
    // index = 0, 1, 2 ... |E|-1
    std::size_t index;
    double      p;
    double      pBoost;

    IMMLink() = default;

    IMMLink(std::size_t from, std::size_t to, std::size_t linkIndex, double p, double pBoost):
            BasicLink(from, to), index(linkIndex), p(p), pBoost(pBoost) {}
};

// Graph type for IMM algorithms
using IMMGraph = graph::Graph<
        IMMNode,
        IMMLink,
        graph::IdentityIndexMap
        >;

// Node type in the PRR-sketch
struct PRRNode : graph::BasicNode<std::size_t> { // NOLINT(cppcoreguidelines-pro-type-member-init)
    // Which state this node will become if no boosted node exists
    // Ca, Cr, or None, according to which kind of message comes first
    NodeState   state = NodeState{};
    // Which state this node will change the center node of its PRR-sketch to
    //  if this node is set as a boosted node
    NodeState   centerStateTo = NodeState{};
    // Minimum distance from any seed node
    int	        dist = 0;
    // Reversed minimum distance from the center node
    int         distR = 0;
    // For all Ca nodes, maxDistP = maximal accepted distance from the nearest seed node
    //  with which this node changes the center node to Ca+ after being boosted.
    int         maxDistP = 0;

    PRRNode() = default;

    explicit PRRNode(std::size_t index, int dist = 0) :
            BasicNode(index), dist(dist) {}
};

struct PRRLink: graph::BasicLink<std::size_t> {
    LinkState   state;

    PRRLink() = default;
    PRRLink(std::size_t from, std::size_t to): BasicLink(from, to), state(LinkState::NotSampledYet) {}
    PRRLink(std::size_t from, std::size_t to, LinkState state): BasicLink(from, to), state(state) {}
};

using PRRGraphBase = graph::Graph<
        PRRNode, PRRLink,
        graph::LinearIndexMap
>;

// Graph type of the PRR-sketch
struct PRRGraph: PRRGraphBase {
    // Index of the center node
    std::size_t center{};
    // State of the center node, equivalent to .centerNode().state
    NodeState   centerState{};

    // Delayed reservation
    explicit PRRGraph(graph::tags::ReservesLater later): PRRGraphBase(later) {}
    // Reserves WITH the arguments
    explicit PRRGraph(const std::map<std::string, std::size_t>& reserveArgs): PRRGraphBase(reserveArgs) {}

    PRRNode& centerNode() {
        return operator[](center);
    }

    [[nodiscard]] const PRRNode& centerNode() const {
        return operator[](center);
    }
};


/*!
 * @brief A collection of link states (Active, Boosted, Blocked).
 *
 * Each object contains a timestamp value T, and a series of timestamps t[i] per link (where i = link index).
 * Each time trying to get the state of a link, its timestamp t[i] is checked first.
 * If its timestamp is not up-to-date (i.e. t[i] != T), refreshes its state
 * and then let t[i] = T marking its state is updated.
 *
 * Refreshing all the states is simplified as lazy modification by incrementing T.
 */
class IMMLinkStateSamples {
    unsigned                globalTimestamp;
    std::vector<unsigned>   timestamps;
    std::vector<LinkState>  linkStates;

public:
    /*!
     * @brief Default constructor. Initialization shall be performed later.
     */
    IMMLinkStateSamples(): globalTimestamp(1) {}

    /*!
     * @brief Constructs with the graph size |E|. See init(n) for details.
     * @param nLinks Graph size |E|
     */
    explicit IMMLinkStateSamples(std::size_t nLinks) { // NOLINT(cppcoreguidelines-pro-type-member-init)
        init(nLinks);
    }

    /*!
     * @brief Initializes with the graph size |E|.
     *
     * The states of each link are left as an unsampled state.
     *
     * @param nLinks Graph size |E|
     */
    void init(std::size_t nLinks) {
        globalTimestamp = 1;
        timestamps.assign(nLinks, 0);
        linkStates.assign(nLinks, LinkState::NotSampledYet);
    }

    /*!
     * @brief Initializes with the graph size |E|. If already initialized, simply performs refreshing.
     *
     * @param nLinks Graph size |E|
     */
    void initOrRefresh(std::size_t nLinks) {
        if (this->nLinks() != nLinks) {
            init(nLinks);
        } else {
            refresh();
        }
    }

    /*!
     * @brief Gets the state of the given link.
     *
     * Index of the link must be in [0, |E|).
     * If it's never sampled before or its state is outdated, resampling is performed.
     *
     * @param link
     * @return The state (Active, Boosted, Blocked) of the link.
     */
    LinkState get(const IMMLink& link) {
        if (timestamps[link.index] != globalTimestamp) {
            timestamps[link.index] = globalTimestamp;
            linkStates[link.index] = getRandomState(link.p, link.pBoost);
        }
        return linkStates[link.index];
    }

    /*!
     * @brief Gets the state of the given link, assuming its state is sampled up-to-date.
     *
     * This version is useful for optimization if get(link) has been called before
     * (so that its state must be up-to-date).
     *
     * @param link
     * @return The state (Active, Boosted, Blocked) of the link.
     */
    [[nodiscard]] LinkState fastGet(const IMMLink& link) const {
        return linkStates[link.index];
    }

    /*!
     * @brief Refreshes all the link states.
     *
     * Lazy strategy is performed by simply incrementing the "global" timestamp T.
     */
    void refresh() {
        globalTimestamp += 1;
    }

    /*!
     * @brief Gets the number of links in this state collection object.
     * @return Graph size |E| in this object.
     */
    [[nodiscard]] std::size_t nLinks() const {
        return linkStates.size();
    }
};

#endif //DAWNSEEKER_GRAPHBASIC_H
