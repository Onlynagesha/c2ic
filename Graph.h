//
// Created by dawnseeker on 2022/3/23.
//

#ifndef DAWNSEEKER_GRAPH_H
#define DAWNSEEKER_GRAPH_H

#include <algorithm>
#include <cassert>
#include <compare>
#include <iostream>
#include <map>
#include <ranges>
#include <string>
#include <type_traits>
#include <vector>

namespace graph {
    namespace tags {
        enum class DoCheck { Yes, No };
        enum class IsBidirectional { Yes, No };
        enum class IsConst { Yes, No };

        // Optimizer switch for Graph type
        enum class EnablesFastAccess { Yes, No };

        // Tag type for Graph type instances that require memory reservation
        struct ReservesLater {};
        inline constexpr ReservesLater reservesLater = ReservesLater{};
    }

    // A basic node type that contains simply the index, WITH index() function provided
    // You can obtain index() for your own node type by inheriting BasicNode
    template <class IndexType>
    class BasicNode {
        IndexType _index;

    public:
        BasicNode() = default;
        explicit BasicNode(IndexType idx): _index(std::move(idx)) {}

        friend const IndexType& index(const std::derived_from<BasicNode<IndexType>> auto& node) {
            return node._index;
        }

        [[nodiscard]] const IndexType& index() const {
            return _index;
        }
    };

    // A basic link type that contains simply the node indices, WITH index1() and index2()
    // You can obtain index1() and index2() for your own node by inheriting BasicLink
    template <class IndexType>
    class BasicLink {
        IndexType _from;
        IndexType _to;

    public:
        BasicLink() = default;
        BasicLink(IndexType u, IndexType v): _from(std::move(u)), _to(std::move(v)) {}

        friend const IndexType& index1(const std::derived_from<BasicLink<IndexType>> auto& link) {
            return link._from;
        }
        friend const IndexType& index2(const std::derived_from<BasicLink<IndexType>> auto& link) {
            return link._to;
        }

        const IndexType& from() const {
            return _from;
        }
        const IndexType& to() const {
            return _to;
        }
        const IndexType& v1() const {
            return _from;
        }
        const IndexType& v2() const {
            return _to;
        }
    };

    // Identity index for unsigned integer types
    template <std::unsigned_integral I>
    inline I index(I i) {
        return i;
    }

    // Identity index for C-style null-terminated string
    template <std::integral CharT>
    inline const CharT* index(const CharT* str) {
        return str;
    }

    // Either node type or index type
    // A index(t) function that gets its index shall be provided
    // index(t) may be searched via argument-dependent lookup (ADL)
    template <class T>
    concept NodeOrIndex = requires(T t) {
        { index(t) };
    };

    // Either node type of index type,
    //  WITH constraint that the index type must be unsigned integer (e.g. std::size_t)
    template <class T>
    concept NodeOrUnsignedIndex = requires(T t) {
        // Use operator + to transform it as a rvalue
        { index(t) + 0u } -> std::unsigned_integral;
    };

    // Link type. Two functions index1(t) and index2(t) shall be provided for its node indices.
    // For a directed link t: u->v,
    //  index1(t) returns the source node u, and
    //  index2(t) returns the target link v
    // For a bidirectional link t: x--y
    //  index1(t) returns either x or y
    //  index2(t) returns either y or x
    template <class T>
    concept LinkType = requires(T t) {
        { index1(t) };
        { index2(t) };
    };

    // The link type WITH the constraint that
    //  both node indices shall be unsigned integer (e.g. std::size_t)
    template <class T>
    concept LinkWithUnsignedIndex = requires(T t) {
        { index1(t) } -> std::unsigned_integral;
        { index2(t) } -> std::unsigned_integral;
    };

    // Index map that for a graph G(V, E),
    //  it maps each node index to integers 0, 1 ... |V|-1 as std::size_t.
    // The mapped indices are provided by the Graph type. (see below for details)
    template <class IndexMap>
    concept IndexMapType = std::is_default_constructible_v<IndexMap>
                           && requires(IndexMap im, std::size_t n, BasicNode<std::size_t> node) {
        // .get(node) returns the mapped index
        { im.get(n) } -> std::convertible_to<std::size_t>;
        { im.get(node) } -> std::convertible_to<std::size_t>;
        // .check(node) returns whether the node has a valid mapped index
        //  which typically means a mapped index has been allocated for the node
        { im.check(n) } -> std::convertible_to<bool>;
        { im.check(node) } -> std::convertible_to<bool>;
        // .set(node, i) sets the mapped index of given node to i
        { im.set(n, n) };
        { im.set(node, n) };
    };

    /*
     * Additional member functions recommended for an IndexMap type:
     * .fastGet(n) and .fastGet(node): returns the mapped index,
     *  assuming the valid index is ensured to exist,
     *  for the sake of higher performance by preventing redundant checks
     * .fastSet(n, i) and .fastSet(node, i): sets the mapped index of node as i,
     *  assuming space has been reserved well
     * .reserve(args) performs memory pre-allocation, WITH args: std::map<std::string, std::size_t>
     * .clear() clears the index map to initial state
     * .reserveClear() clears the index map, WITH the last reservation still works,
     *  which typically implies that no memory is deallocated
     */

    /*
     * Identity map. Simply returns index(node) itself for each .get(node) call.
     * It's required that into the graph the nodes are added in the order of index = 0, 1, 2...
     */
    class IdentityIndexMap {
        // The next mapped index, also the current size of Graph |V|
        std::size_t nextIndex{};

    public:
        // Resets index count.
        void clear() {
            nextIndex = 0;
        }

        // Resets index count.
        void reserveClear() {
            nextIndex = 0;
        }

        // Gets the index as its identity
        std::size_t get(const NodeOrUnsignedIndex auto& node) const {
            return index(node);
        }

        // Checks whether the index is in 0, 1 ... |V|-1
        std::size_t check(const NodeOrUnsignedIndex auto& node) const {
            return index(node) < nextIndex;
        }

        // Simply adds index count by 1.
        void set(const NodeOrUnsignedIndex auto& node, std::size_t mappedIndex) {
            assert(mappedIndex == nextIndex);
            nextIndex += 1;
        }
    };

    // Index map from size_t to size_t
    // Let I be the maximal original index,
    //  this data structure needs a linear list of size O(I) to store all the mapped indices.
    // For check-free graph methods, reservation on maxIndex is required.
    class SemiSparseIndexMap {
    private:
        // Null index, _map[v] == null indicates that node v has not been allocated a mapped index
        static constexpr std::size_t null = static_cast<std::size_t>(-1);
        // Linear list as the index map
        std::vector<std::size_t> _map;

    public:
        SemiSparseIndexMap() = default;

        // Reserves the linear list by maximal original index
        void reserve(const std::map<std::string, std::size_t>& args) {
            auto it = args.find("maxIndex");
            if (it == args.end()) {
                std::cerr << "WARNING on SemiSparseIndexMap::reserve: argument 'maxIndex' is not provided!"
                          << std::endl;
                assert(!"argument 'maxIndex' is not provided");
            }
            // Fills all un-accessed places to null
            _map.resize(it->second + 1, null);
        }

        // Resets the index map
        void clear() {
            _map.clear();
        }

        // Resets the index map, WITH _map still reserved by the maxIndex
        void reserveClear() {
            std::ranges::fill(_map, null);
        }

    private:
        // Internal helper function to allocate and get a mapped index
        std::size_t _get(std::size_t id) {
            if (id >= _map.size()) {
                _map.resize(id + 1, null);
            }
            return _map[id];
        }

    public:
        // Gets the mapped index
        [[nodiscard]] std::size_t get(const NodeOrUnsignedIndex auto& node) const {
            // This method shall behave as if it's const-qualified.
            // However, the internal data structure may change if an index allocation is triggered
            return const_cast<SemiSparseIndexMap*>(this)->_get(index(node));
        }

        // Gets the mapped index, assuming the map index always exist
        std::size_t fastGet(const NodeOrUnsignedIndex auto& node) const {
            return _map[index(node)];
        }

        // Check whether the index has been assigned a mapped one
        [[nodiscard]] bool check(const NodeOrUnsignedIndex auto& node) const {
            return index(node) < _map.size() && _map[index(node)] != null;
        }

        // Sets the mapped index of given node
        void set(const NodeOrUnsignedIndex auto& node, std::size_t mappedIndex) {
            auto origIndex = index(node);
            if (origIndex >= _map.size()) {
                _map.resize(origIndex + 1, null);
            }
            _map[origIndex] = mappedIndex;
        }

        // Sets the mapped index of given node, assuming space of _map is reserved well
        void fastSet(const NodeOrUnsignedIndex auto& node, std::size_t mappedIndex) {
            _map[index(node)] = mappedIndex;
        }
    };

    // Template parameters of Graph G(V,E):
    // Node: type of the node, index() function shall be provided for its index (before mapping)
    // Link: type of the link, index1() and index2() for its node indices (before mapping)
    // IndexMap: type of the index map
    // enablesFastAccess: whether enable fast access to the adjacency list items.
    //  If enabled, reserve({{"nodes", |V|}, {"links", |E|}}) must be performed.
    template <NodeOrIndex Node,
            LinkType Link,
            IndexMapType IndexMap = SemiSparseIndexMap,
            tags::EnablesFastAccess enablesFastAccess = tags::EnablesFastAccess::No>
    class Graph {
        // If both nodes and links are preserved,
        //  a faster-access type for adjacency list item can be enabled.
        static constexpr bool enablesFastRefLink =
                enablesFastAccess == tags::EnablesFastAccess::Yes;

        // Item type in the adjacency list by default.
        struct IndexRefLink {
            std::size_t to;
            std::size_t link;
        };

        // Item type in the adjacency list if fast-access is enabled
        using RefLink = std::pair<Node&, Link&>;
        using ConstRefLink = std::pair<const Node&, const Link&>;

        // Adjacency list of the graph, along WITH an inverse one for the transposed graph.
        // All the indices are mapped results of the index map.
        // For directed links,
        //  _adjList[u] contains all the links that starts from the node u
        //  _invAdjList[u] contains all the links that targets u
        // For bidirectional links,
        //  both _adjList[u] and _invAdjList[u] contain all the links WITH u as an end
        // If without fast-access enabled, each item in _adjList[u] is an index pair {v, l}
        //  referring to the link u -> v (or u -- v) WITH link index = l.
        // If fast-access enabled, each item in _adjList[u] is a reference pair {_nodes[v], _links[l]}
        using AdjList = std::vector<std::conditional_t<
                enablesFastRefLink, RefLink, IndexRefLink>
                >;
        using ConstAdjList = std::vector<std::conditional_t<
                enablesFastRefLink, ConstRefLink, IndexRefLink>
                >;

        // Linear list of the node objects
        std::vector<Node>       _nodes;
        // Linear object of the link objects
        std::vector<Link>       _links;
        // Adjacency list
        std::vector<AdjList>    _adjList;
        // Inverse adjacency list
        std::vector<AdjList>    _invAdjList;
        // Index map object
        IndexMap                _indexMap;

        // Helper function to get the mapped node index without checking
        // If _indexMap.fastGet() is not provided, use _indexMap.get() alternatively
        std::size_t _fastGet(const NodeOrIndex auto& node) const {
            if constexpr(requires {_indexMap.fastGet(node);}) {
                return _indexMap.fastGet(node);
            } else {
                return _indexMap.get(node);
            }
        }

        // Helper function to get the first index of a link without checking
        std::size_t _fastGet1(const LinkType auto& link) const {
            if constexpr(requires {_indexMap.fastGet(link);}) {
                return _indexMap.fastGet(index1(link));
            } else {
                return _indexMap.get(index1(link));
            }
        }

        // Helper function to get the second index of a link without checking
        std::size_t _fastGet2(const LinkType auto& link) const {
            if constexpr(requires {_indexMap.fastGet(link);}) {
                return _indexMap.fastGet(index2(link));
            } else {
                return _indexMap.get(index2(link));
            }
        }

        // Helper function to check the two nodes of a link
        bool _checkLink(const LinkType auto& link) const {
            return _indexMap.check(index1(link)) && _indexMap.check(index2(link));
        }

        // Helper function to set the mapped index WITH the index map
        //  assuming space in the index map has been reserved well.
        // The .fastSet() alternative is used if supported.
        void _fastSet(const NodeOrIndex auto& node, std::size_t mappedIndex) {
            if constexpr(requires {_indexMap.fastSet(node, mappedIndex);}) {
                _indexMap.fastSet(node, mappedIndex);
            } else {
                _indexMap.set(node, mappedIndex);
            }
        }

        // Helper function to add a node.
        // If the node has never been added before, mapped index = |V| - 1 after adding.
        // Otherwise, the old node WITH the same index will be replaced.
        // Returns a pointer to the node added
        template <tags::DoCheck doCheck>
        Node* _addNode(Node node) {
            // |V| - 1 after adding
            auto mappedIndex = _nodes.size();

            if constexpr (doCheck == tags::DoCheck::Yes) {
                // Checks whether some other node WITH the same index has been added before
                // If added, then replace it WITH the current node
                if (_indexMap.check(node)) {
                    auto existedIndex = _fastGet(node);
                    _nodes[existedIndex] = std::move(node);
                    // Returns a pointer to the node added
                    return _nodes.data() + existedIndex;
                }
                // Otherwise, mapped index = |V| - 1
                _indexMap.set(node, mappedIndex);
                // Checks the size of adjacency list
                if (mappedIndex >= _adjList.size()) {
                    _adjList.resize(mappedIndex + 1);
                    _invAdjList.resize(mappedIndex + 1);
                }
            } else {
                // If checking is disabled, it assumes that the node has never been added before
                //  so that a new index is always assigned.
                _fastSet(node, mappedIndex);
            }
            // Returns a pointer to the node added
            return (_nodes.push_back(std::move(node)), _nodes.data() + mappedIndex);
        }

        // Helper function to create an adjacency list item {v, iLink} or {_nodes[v], _links[iLink}
        auto _makeRefLink(std::size_t v, std::size_t iLink) {
            if constexpr (enablesFastRefLink) {
                return RefLink(_nodes[v], _links[iLink]);
            } else {
                return IndexRefLink{v, iLink};
            }
        }

        // Adds a link object WITH mapped indices u -> v (or u -- v)
        // Returns a pointer to the added link object
        template <tags::DoCheck doCheck, tags::IsBidirectional isBidirectional>
        Link* _addLink(std::size_t u, std::size_t v, Link link) {
            // Checks the adjacency list size
            if constexpr(doCheck == tags::DoCheck::Yes) {
                auto n = std::max(u, v);
                if (n >= _adjList.size()) {
                    _adjList.resize(n + 1);
                    _invAdjList.resize(n + 1);
                }
            }
            // Assigns the link index to current |E|-1
            auto idx = _links.size();
            auto res = (_links.push_back(std::move(link)), _links.data() + idx);

            // For directed link, adds u -> v
            _adjList[u].push_back(_makeRefLink(v, idx));
            _invAdjList[v].push_back(_makeRefLink(u, idx));

            // For bidirectional link, it's added as two directed links u -> v and v -> u
            //  that refers to the same link object
            if constexpr (isBidirectional == tags::IsBidirectional::Yes) {
                _adjList[v].push_back(_makeRefLink(u, idx));
                _invAdjList[u].push_back(_makeRefLink(v, idx));
            }

            return res;
        }

        // Helper function to find a link WITH mapped index u -> v (or u -- v)
        // Returns a pointer to the link object found, or nullptr if not found
        Link* _findLink(std::size_t u, std::size_t v) const {
            for (const auto& e: _adjList[u]) {
                if constexpr (enablesFastRefLink) {
                    // {.to = ref. of _nodes[v], .link = ref. of _links[iLink]}
                    if (index(e.to) == v) {
                        return &e.link;
                    }
                } else {
                    // {.to = v, link = iLink}
                    if (e.to == v) {
                        return _links.data() + e.link;
                    }
                }
            }
            // Not found
            return nullptr;
        }

    public:
        // Default constructor is enabled only if fast access is not used
        Graph() requires (!enablesFastRefLink) = default;

        // Constructor WITH a tags::reserveLater tag
        //  to indicate the reserve operation is performed later
        explicit Graph(tags::ReservesLater) {}

        // Constructor WITH reserve arguments provided as std::map or an initializer_list
        //  e.g. { {"nodes", 10}, {"links", 20}, {"maxIndex", 30} }
        explicit Graph(const std::map<std::string, std::size_t>& reserveArgs) {
            reserve(reserveArgs);
        }

    private:
        // Helper function deep-copying the adjacency lists whose items are references
        // Migrates all the references to the copied this->_nodes and this->_links
        auto _deepCopy(const Graph& other, std::vector<AdjList> Graph::* which) {
            auto nodesHead = other._nodes.data();
            auto linksHead = other._links.data();

            // Target adjacency list
            auto res = std::vector<AdjList>((other.*which).size());
            for (std::size_t i = 0; i < res.size(); i++) {
                const auto& fromList = (other.*which)[i];
                auto& destList = res[i];

                if (fromList.empty()) {
                    continue;
                }
                destList.reserve(fromList.size());

                for (const auto& item: fromList) {
                    // Extracts indices by subtracting the two pointers
                    auto nodeIndex = &item.first - nodesHead;
                    auto linkIndex = &item.second - linksHead;
                    // Maps indices to references to the objects in this graph
                    destList.push_back(RefLink(_nodes[nodeIndex], _links[linkIndex]));
                }
            }
            return res;
        }

    public:
        // Copy constructor. For fast-access adjacency list items,
        //  a _deepCopy() process is required.
        Graph(const Graph& other):
        _nodes(other._nodes), _links(other._links), _indexMap(other._indexMap) {
            if constexpr (enablesFastRefLink) {
                _adjList = _deepCopy(other, &Graph::_adjList);
                _invAdjList = _deepCopy(other, &Graph::_invAdjList);
            } else {
                _adjList = other._adjList;
                _invAdjList = other._invAdjList;
            }
        }

    private:
        // Helper function to reset the graph
        //  and index map (if _indexMap.clear() is provided)
        void _clear() {
            _nodes.clear();
            _links.clear();
            _adjList.clear();
            _invAdjList.clear();
            if constexpr (requires { _indexMap.clear(); }) {
                _indexMap.clear();
            }
        }

    public:
        // Resets the graph and deallocates memory space
        // Only available if fast access is not enabled
        void clear()
        requires(!enablesFastRefLink) {
            _clear();
        }

        // Resets the graph and deallocates memory space
        void clear(tags::ReservesLater) {
            _clear();
        }

        // Resets the graph, memory space preserved
        // The last .reserve(args) call still works.
        void reserveClear() {
            _nodes.clear();
            _links.clear();
            for (auto& e: _adjList) {
                e.clear();
            }
            for (auto& e: _invAdjList) {
                e.clear();
            }
            if constexpr (requires { _indexMap.reserveClear(); }) {
                _indexMap.reserveClear();
            }
        }

    public:
        // Reserves memory space
        // Arguments are provided as a std::map or std::initializer_list
        //  like { {"nodes", 10}, {"links", 30}, {"maxIndex", 30} }
        void reserve(const std::map<std::string, std::size_t>& args) {
            // For fast access, reservation for |V| and |E| is necessary
            if constexpr(enablesFastRefLink) {
                if (!args.contains("nodes") || !args.contains("links")) {
                    std::cerr << "WARNING on Graph::reserve: arguments 'nodes' and 'links' must "
                                 "be provided for fast access enabled graph!" << std::endl;
                    assert(0);
                }
            }
            // Reserves the index map
            if constexpr(requires {_indexMap.reserve(args);}) {
                _indexMap.reserve(args);
            }
            // Reserves the graph
            for (const auto& [varName, value]: args) {
                if (varName == "nodes") {
                    _nodes.reserve(value);
                    _adjList.resize(value);
                    _invAdjList.resize(value);
                } else if (varName == "links") {
                    _links.reserve(value);
                }
            }
        }

        // Adds a node. If some other node WITH the same index exists,
        //  that node will be replaced by the current one.
        // Returns a pointer to the node added.
        Node* addNode(Node node) {
            return _addNode<tags::DoCheck::Yes>(std::move(node));
        }

        // Adds a node, assuming the node is always new, i.e. no other node WITH the same index
        // Returns a pointer to the node added.
        Node* fastAddNode(Node node) {
            return _addNode<tags::DoCheck::No>(std::move(node));
        }

        // Adds a directed link.
        // It's required that both nodes have been added before. Otherwise, fails to add.
        // Returns a pointer to the link added if success, otherwise nullptr.
        Link* addLink(Link link) {
            if (!_checkLink(link)) {
                return nullptr;
            }
            return _addLink<tags::DoCheck::Yes, tags::IsBidirectional::No>(
                    _fastGet1(link), _fastGet2(link), std::move(link)
            );
        }

        // Adds a directed link, assuming that:
        // (1) both nodes always exist,
        // (2) maximal number of nodes has been reserved well by providing "nodes" argument in .reserve()
        // Returns a pointer to the link added.
        Link* fastAddLink(Link link) {
            return _addLink<tags::DoCheck::No, tags::IsBidirectional::No>(
                    _fastGet1(link), _fastGet2(link), std::move(link)
            );
        }

        // Adds a bidirectional link, required that both nodes exist, fails otherwise.
        // Returns a pointer to the link added if success, otherwise nullptr.
        Link* addBiLink(Link link) {
            if (!_checkLink(link)) {
                return nullptr;
            }
            return _addLink<tags::DoCheck::Yes, tags::IsBidirectional::Yes>(
                    _fastGet1(link), _fastGet2(link), std::move(link)
            );
        }

        // Adds a bidirectional link, assuming that:
        // (1) both nodes always exist,
        // (2) maximal number of nodes has been reserved well by providing "nodes" argument in .reserve()
        // Returns a pointer to the link added.
        Link* fastAddBiLink(Link link) {
            return _addLink<tags::DoCheck::No, tags::IsBidirectional::Yes>(
                    _fastGet1(link), _fastGet2(link), std::move(link)
            );
        }

        // Number of nodes
        [[nodiscard]] std::size_t nNodes() const {
            return _nodes.size();
        }

        // Number of links
        [[nodiscard]] std::size_t nLinks() const {
            return _links.size();
        }

        // Tests whether a node has been added
        bool hasNode(const NodeOrIndex auto& node) const {
            return _indexMap.check(node);
        }

        // In-degree of a node, i.e. how many links target to it.
        // If the node has never been added, then returns 0.
        std::size_t inDegree(const NodeOrIndex auto& to) const {
            return _indexMap.check(to) ? fastInDegree(to) : 0;
        }

        // In-degree of a node, assuming that:
        // (1) the node has been added before;
        // (2) maximal number of nodes have been reserved well by "nodes" argument in .reserve() call.
        std::size_t fastInDegree(const NodeOrIndex auto& to) const {
            return _invAdjList[_fastGet(to)].size();
        }

        // Out-degree of a node, i.e. how many links come out from it.
        // If the node has never been added, then returns 0.
        std::size_t outDegree(const NodeOrIndex auto& from) const {
            return _indexMap.check(from) ? fastOutDegree(from) : 0;
        }

        // Out-degree of a node, assuming that:
        // (1) the node has been added before;
        // (2) maximal number of nodes have been reserved well by "nodes" argument in .reserve() call.
        std::size_t fastOutDegree(const NodeOrIndex auto& from) const {
            return _adjList[_fastGet(from)].size();
        }

        // Gets a pointer of the node WITH given index. (non-const overload)
        // If the node does not exist, returns nullptr.
        Node* node(const NodeOrIndex auto& idx) {
            return hasNode(idx) ? fastNode(idx) : nullptr;
        }

        // Gets a pointer of the node WITH given index. (const-qualified overload)
        // If the node does not exist, returns nullptr.
        const Node* node(const NodeOrIndex auto& idx) const {
            return const_cast<Graph*>(this)->node(idx);
        }

        // Gets a pointer to the node WITH given index. (non-const overload)
        // Assumes the node always exists.
        Node* fastNode(const NodeOrIndex auto& idx) {
            return _nodes.data() + _fastGet(idx);
        }

        // Gets a pointer to the node WITH given index. (const-qualified overload)
        // Assumes the node always exists.
        const Node* fastNode(const NodeOrIndex auto& idx) const {
            return const_cast<Graph*>(this)->fastNode(idx);
        }

        // Similar to .fastNode(), a reference to the node is returned. (non-const overload)
        Node& operator [] (const NodeOrIndex auto& idx) {
            return _nodes[_fastGet(idx)];
        }

        // Similar to .fastNode(), a reference to the node is returned. (const-qualified overload)
        const Node& operator [] (const NodeOrIndex auto& idx) const {
            return _nodes[_fastGet(idx)];
        }

        // Gets a pointer to the link u -> v or u -- v WITH given node indices,
        // or nullptr when it doesn't exist. (non-const overload)
        // Checks whether both nodes exist. If not, returns nullptr.
        Link* link(const NodeOrIndex auto& u, const NodeOrIndex auto& v) {
            // Note that both u and v exist does not always indicate the link exists.
            // nullptr is still possible to get returned.
            return _indexMap.check(u) && _indexMap.check(v)
                   ? fastLink(_fastGet(u), _fastGet(v))
                   : nullptr;
        }

        // Gets a pointer to the link u -> v or u -- v WITH given node indices,
        // or nullptr when it doesn't exist. (const-qualified overload)
        // Checks whether both nodes exist. If not, returns nullptr.
        const Link* link(const NodeOrIndex auto& u, const NodeOrIndex auto& v) const {
            return const_cast<Graph*>(this)->link(u, v);
        }

        // Gets a pointer to the link u -> v or u -- v WITH given node indices.
        // or nullptr if it doesn't exist. (non-const overload)
        // Fewer checks assuming that both nodes exist.
        Link* fastLink(const NodeOrIndex auto& u, const NodeOrIndex auto& v) {
            return _findLink(_fastGet(u), _fastGet(v));
        }

        // Gets a pointer to the link u -> v or u -- v WITH given node indices.
        // or nullptr if it doesn't exist. (const-qualified overload)
        // Fewer checks assuming that both nodes exist.
        const Link* fastLink(const NodeOrIndex auto& u, const NodeOrIndex auto& v) const {
            return const_cast<Graph*>(this)->fastLink(u, v);
        }

        // Gets a view to all the nodes. (non-const overload)
        auto nodes() {
            return std::ranges::subrange(_nodes);
        }

        // Gets a view to all the nodes. (const-qualified overload)
        auto nodes() const {
            return std::ranges::subrange(_nodes);
        }

        // Gets a view to all the links. (non-const overload)
        auto links() {
            return std::ranges::subrange(_links);
        }

        // Gets a view to all the links. (const-qualified overload)
        auto links() const {
            return std::ranges::subrange(_links);
        }

    private:
        enum class Which { From, To };

        // Helper function to wrap the given range (i.e. AdjList)
        template <class ThisPtr>
        static auto _wrap(const AdjList& r, ThisPtr ptr) {
            constexpr bool isConst = std::is_const_v<std::remove_pointer_t<ThisPtr>>;
            if constexpr (enablesFastRefLink) {
                // RefLink: {node, link} reference pair
                if constexpr (isConst) {
                    // For const-qualified call, transforms all the contents to const-reference.
                    return std::ranges::subrange(reinterpret_cast<const ConstAdjList&>(r));
                } else {
                    return std::ranges::subrange(r);
                }
            } else {
                // IndexRefLink: {v, iLink} index pair
                auto func = [ptr](const IndexRefLink& ref) {
                    using ResultType = std::conditional_t<isConst, ConstRefLink, RefLink>;
                    // Transform indices to references
                    return ResultType(ptr->_nodes[ref.to], ptr->_links[ref.link]);
                };
                return r | std::views::transform(func);
            }
        }

        // Helper function to get the view of given range, i.e. AdjList.
        template <Which which, tags::DoCheck doCheck, class ThisPtr>
        static auto _linksWith(ThisPtr ptr, const NodeOrIndex auto& with) {
            static auto emptyList = AdjList();

            // If the node does not exist, returns the empty list
            if constexpr (doCheck == tags::DoCheck::Yes) {
                if (!ptr->_indexMap.check(with) || ptr->_fastGet(with) >= ptr->_adjList.size()) {
                    return _wrap(emptyList, ptr);
                }
            }
            if constexpr (which == Which::From) {
                return _wrap(ptr->_adjList[ptr->_fastGet(with)], ptr);
            } else {
                return _wrap(ptr->_invAdjList[ptr->_fastGet(with)], ptr);
            }
        }

    public:
        // Gets a view of all the links from given node.
        // If the node does not exist, returns an empty view.
        // Returns a view of {to, link} non-const reference pairs.
        auto linksFrom(const NodeOrIndex auto& from) {
            return _linksWith<Which::From, tags::DoCheck::Yes>(this, from);
        }

        // Gets a view of all the links from given node.
        // If the node does not exist, returns an empty view.
        // Returns a view of {to, link} const-qualified reference pairs.
        auto linksFrom(const NodeOrIndex auto& from) const {
            return _linksWith<Which::From, tags::DoCheck::Yes>(this, from);
        }

        // Gets a view of all the links from given node, assuming that:
        // (1) the node always exists;
        // (2) maximal number of nodes has been reserved well by "nodes" argument in .reserve() call
        // Returns a view of {to, link} non-const reference pairs.
        auto fastLinksFrom(const NodeOrIndex auto& from) {
            return _linksWith<Which::From, tags::DoCheck::No>(this, from);
        }

        // Gets a view of all the links from given node, assuming that:
        // (1) the node always exists;
        // (2) maximal number of nodes has been reserved well by "nodes" argument in .reserve() call
        // Returns a view of {to, link} const-qualified reference pairs.
        auto fastLinksFrom(const NodeOrIndex auto& from) const {
            return _linksWith<Which::From, tags::DoCheck::No>(this, from);
        }

        // Gets a view of all the links to the given node.
        // If the node does not exist, returns an empty view.
        // Returns a view of {from, link} non-const reference pairs.
        auto linksTo(const NodeOrIndex auto& to) {
            return _linksWith<Which::To, tags::DoCheck::Yes>(this, to);
        }

        // Gets a view of all the links to the given node.
        // If the node does not exist, returns an empty view.
        // Returns a view of {from, link} const-qualified reference pairs.
        auto linksTo(const NodeOrIndex auto& to) const {
            return _linksWith<Which::To, tags::DoCheck::Yes>(this, to);
        }


        // Gets a view of all the links to the given node, assuming that:
        // (1) the node always exists;
        // (2) maximal number of nodes has been reserved well by "nodes" argument in .reserve() call
        // Returns a view of {from, link} non-const reference pairs.
        auto fastLinksTo(const NodeOrIndex auto& to) {
            return _linksWith<Which::To, tags::DoCheck::No>(this, to);
        }

        // Gets a view of all the links to the given node, assuming that:
        // (1) the node always exists;
        // (2) maximal number of nodes has been reserved well by "nodes" argument in .reserve() call
        // Returns a view of {from, link} const-qualified reference pairs.
        auto fastLinksTo(const NodeOrIndex auto& to) const {
            return _linksWith<Which::To, tags::DoCheck::No>(this, to);
        }
    };
}

#endif //DAWNSEEKER_GRAPH_H
