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

        struct ReservesLater {};
        inline constexpr ReservesLater reservesLater = ReservesLater{};
    }

    // A basic node type that contains simply the index, with index() function provided
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

    // A basic link type that contains simply the node indices, with index1() and index2()
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
    //  with constraint that the index type must be unsigned integer (e.g. std::size_t)
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

    // The link type with the constraint that
    //  both node indices shall be unsigned integer (e.g. std::size_t)
    template <class T>
    concept LinkTypeWithUnsignedIndex = requires(T t) {
        { index1(t) } -> std::unsigned_integral;
        { index2(t) } -> std::unsigned_integral;
    };

    // Reserve strategy set of a graph, in the form of bit-mask
    // Nodes: reserves the number of nodes
    // Links: reserves the number of links
    // MaxIndex: for unsigned integer indices, reserves the maximum possible index of nodes
    enum class ReserveStrategy {
        None = 0,
        Nodes = 1,
        Links = 2,
        MaxIndex = 4,
        All = 7
    };

    // Support of set union operator
    constexpr ReserveStrategy operator | (ReserveStrategy A, ReserveStrategy B) {
        return (ReserveStrategy)((int)A | (int)B);
    }

    // Partial-order comparison of the two bit-mask sets
    // A == B: A and B are exactly the same
    // A != B: A and B are not the same
    // A <= B: A is subset of B
    // A <  B: A is proper subset of B
    // A >= B: A is superset of B (or, B is subset of A)
    // A >  B: A is proper superset of B (or, B is proper subset of A)
    // NOTE ON PARTIAL ORDER: (A <= B) does not always indicate (A > B),
    //  when neither A nor B is subset of another.
    constexpr std::partial_ordering operator <=> (ReserveStrategy A, ReserveStrategy B) {
        int a = (int)A;
        int b = (int)B;
        if (a == b) {
            return std::partial_ordering::equivalent;
        }
        if ((a & b) == a) {
            return std::partial_ordering::less;
        }
        if ((a & b) == b) {
            return std::partial_ordering::greater;
        }
        return std::partial_ordering::unordered;
    }

    // Index map that for a graph G(V, E),
    //  it maps each node index to integers 0, 1 ... |V|-1 as std::size_t
    template <class IndexMap>
    concept IndexMapType = std::is_default_constructible_v<IndexMap>
                           && requires(IndexMap im, std::size_t n, BasicNode<std::size_t> node) {
        // .get(node) returns the mapped index
        // If node has never been queried before, a new index is allocated for it
        { im.get(n) } -> std::convertible_to<std::size_t>;
        { im.get(node) } -> std::convertible_to<std::size_t>;
        // .check(node) returns whether the node has a valid mapped index
        //  which typically means whether the index map has allocated an index for the node
        { im.check(n) } -> std::convertible_to<bool>;
        { im.check(node) } -> std::convertible_to<bool>;
        // .size() returns total number of nodes that has been allocated an index
        //  which typically equals to the number of nodes queried
        { im.size() } -> std::convertible_to<std::size_t>;
    };

    /*
     * Additional member functions recommended for an IndexMap type:
     * .fastGet(n) and .fastGet(node): returns the mapped index,
     *  assuming the node has been queried before so that a valid index is ensured to exist,
     *  for the sake of higher performance by preventing redundant checks
     * .reserve(args) performs memory pre-allocation, with args: ReserveArguments
     * .clear() clears the index map to initial state
     * .reserveClear() clears the index map, with the last reservation still works,
     *  which typically implies that no memory is deallocated
     *
     * Additional static member constants for an IndexMap type:
     * ::minimalStrategy: minimal reserve strategy such that it can work properly
     *  If provided, a strict restriction is imposed on the graph class
     *  so that it always fails to work without reserving all the required values,
     *  even if methods with index check (without 'fast' index) are used.
     * ::minimalReserveStrategy: minimal reserve strategy such that it can work properly after reservation
     *  If provided, a restriction is imposed on the graph class
     *  so that all check-free methods (with 'fast' prefix) fail to work
     *  without preserving all the required values
     */

    // Aggregation of reserve arguments
    struct ReserveArguments {
    private:
        static constexpr bool _printsErrorInfoToCout = true;

    public:
        static constexpr std::size_t notProvided = 0;

        std::size_t nodes       = notProvided;
        std::size_t links       = notProvided;
        std::size_t maxIndex    = notProvided;

        ReserveArguments() = default;
#pragma clang diagnostic push
#pragma ide diagnostic ignored "google-explicit-constructor"
#pragma ide diagnostic ignored "Simplify"
        // Constructs with a string => size_t map
        ReserveArguments(const std::map<std::string, std::size_t>& map) {
            for (auto& [str, val]: map) {
                if (str == "nodes") {
                    nodes = val;
                } else if (str == "links") {
                    links = val;
                } else if (str == "maxIndex") {
                    maxIndex = val;
                } else if constexpr (_printsErrorInfoToCout) {
                    std::cout
                    << "Warning on ReserveArguments(map): variable name '"
                    << str << "' is not one of 'nodes', 'links' and 'maxIndex'. "
                    << "Please check spelling." << std::endl;
                }
            }
        }
#pragma clang diagnostic pop
    };

    // Extracts the minimal strategy for all the graph methods
    template <class IndexMap>
    constexpr ReserveStrategy minimalStrategyOf() {
        if constexpr(requires {{IndexMap::minimalStrategy};}) {
            return IndexMap::minimalStrategy;
        }
        return ReserveStrategy::None;
    }

    // Extracts the minimal strategy for all the check-free graph methods
    template <class IndexMap>
    constexpr ReserveStrategy minimalReserveStrategyOf() {
        auto res = minimalStrategyOf<IndexMap>();
        if constexpr(requires {{IndexMap::minimalReserveStrategy};}) {
            res = res | IndexMap::minimalReserveStrategy;
        }
        return res;
    }

    // Index identity map with minimal operation
    // Reservation is always required even if graph methods with index check are used,
    //  with the implication that the exact number of nodes shall be known in prior.
    // Common use case is that all the query methods (accessing nodes or links, or querying graph size, etc.)
    //  of the graph class are used only after the whole graph is finished reading.
    class MinimalIndexIdentity {
    public:
        // Requires reserving with the exact number of nodes
        static constexpr ReserveStrategy minimalStrategy = ReserveStrategy::Nodes;

    private:
        // The exact number of nodes
        std::size_t _size;

    public:
        MinimalIndexIdentity() = default;

        // Reserves with the exactly number of nodes
        void reserve(const ReserveArguments& args) {
            assert(args.nodes != ReserveArguments::notProvided);
            _size = args.nodes;
        }

        // Simply identity map, x -> x
        [[nodiscard]] std::size_t get(const NodeOrUnsignedIndex auto& node) const {
            return index(node);
        }

        // No check is performed
        [[nodiscard]] bool check(const NodeOrUnsignedIndex auto& node) const {
            return true;
        }

        // Exact number of nodes
        [[nodiscard]] std::size_t size() const {
            return _size;
        }
    };

    // Index map from size_t to size_t
    // Let I be the maximal original index, |V| be the graph size,
    //  this data structure needs a linear list of size O(I) to store all the mapped indices.
    // For check-free graph methods, reservation on maxIndex is required.
    class SemiSparseIndexMap {
    public:
        // For check-free graph methods, reservation on maxIndex is required
        static constexpr ReserveStrategy minimalReserveStrategy = ReserveStrategy::MaxIndex;

    private:
        // Null index, _map[v] == null indicates that node v has not been allocated a mapped index
        static constexpr std::size_t null = static_cast<std::size_t>(-1);

        // Graph size |V|, the number of nodes that has been allocated an index
        std::size_t _size = 0;
        // Linear list as the index map
        std::vector<std::size_t> _map;

    public:
        SemiSparseIndexMap() = default;

        // Reserves the linear list by maximal original index
        void reserve(const ReserveArguments& args) {
            // Fills all un-accessed places to null
            _map.resize(args.maxIndex + 1, null);
        }

        // Resets the index map
        void clear() {
            _size = 0;
            _map.clear();
        }

        // Resets the index map, with _map still reserved by the maxIndex
        void reserveClear() {
            _size = 0;
            std::ranges::fill(_map, null);
        }

    private:
        // Internal helper function to allocate and/or get a mapped index
        std::size_t _get(std::size_t id) {
            if (id >= _map.size()) {
                _map.resize(id + 1, null);
            }
            // If not allocated before, assign its mapped index to current |V|-1
            if (_map[id] == null) {
                _map[id] = _size++;
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

        // The current graph size |V|
        [[nodiscard]] std::size_t size() const {
            return _size;
        }
    };

    // Template parameters of Graph G(V,E):
    // Node: type of the node, index() function shall be provided for its index (before mapping)
    // Link: type of the link, index1() and index2() for its node indices (before mapping)
    // IndexMap: type of the index map
    // strategy: expected reservation strategy of the graph. Higher strategy enables further optimization.
    //  Must be superset of minimalStrategy of the IndexMap type.
    //  If this parameter is set other than None, reservation is mandatory
    //  before any modification or access operation.
    template <NodeOrIndex Node,
            LinkType Link,
            IndexMapType IndexMap = SemiSparseIndexMap,
            ReserveStrategy strategy = minimalStrategyOf<IndexMap>()>
    requires (strategy >= minimalStrategyOf<IndexMap>())
    class Graph {
        // If both nodes and links are preserved,
        //  a faster-access type for adjacency list item can be enabled.
        static constexpr bool enablesFastRefLink =
                strategy >= (ReserveStrategy::Nodes | ReserveStrategy::Links);

        // Item type in the adjacency list by default.
        struct IndexRefLink {
            std::size_t to;
            std::size_t link;
        };

        // Item type in the adjacency list if fast-access is enabled
        using RefLink = std::pair<Node&, Link&>;
        using ConstRefLink = std::pair<const Node&, const Link&>;

        // Adjacency list of the graph, along with an inverse one for the transposed graph.
        // All the indices are mapped results of the index map.
        // For directed links,
        //  _adjList[u] contains all the links that starts from the node u
        //  _invAdjList[u] contains all the links that targets u
        // For bidirectional links,
        //  both _adjList[u] and _invAdjList[u] contain all the links with u as an end
        // If without fast-access enabled, each item in _adjList[u] is an index pair {v, l}
        //  referring to the link u -> v (or u -- v) with link index = l.
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

        // Helper function to add a node with idx = mapped index
        // Returns a pointer to the node added
        template <tags::DoCheck doCheck>
        Node* _addNode(std::size_t idx, Node node) {
            // Checks node index
            if constexpr(doCheck == tags::DoCheck::Yes) {
                if (idx >= _nodes.size()) {
                    _nodes.resize(idx + 1);
                }
            }
            return (_nodes[idx] = std::move(node), _nodes.data() + idx);
        }

        // Helper function to create an adjacency list item {v, iLink} or {_nodes[v], _links[iLink}
        auto _makeRefLink(std::size_t v, std::size_t iLink) {
            if constexpr (enablesFastRefLink) {
                return RefLink(_nodes[v], _links[iLink]);
            } else {
                return IndexRefLink{v, iLink};
            }
        }

        // Adds a link object with mapped indices u -> v (or u -- v)
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

        // Helper function to find a link with mapped index u -> v (or u -- v)
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
        // Default constructor is enabled only if
        //  no reservation requirements for index-checking methods
        Graph() requires (strategy == ReserveStrategy::None) = default;

        // Constructor with a tags::reserveLater tag
        //  to indicate the reserve operation is performed later
        explicit Graph(tags::ReservesLater) {}

        // Constructor with reserve arguments
        explicit Graph(ReserveArguments reserveArgs) {
            reserve(reserveArgs);
        }

        // Constructor with reserve arguments provided as std::map or an initializer_list
        //  e.g. { {"nodes", 10}, {"links", 20}, {"maxIndex", 30} }
        explicit Graph(const std::map<std::string, std::size_t>& reserveArgs) {
            reserve(ReserveArguments(reserveArgs));
        }

    private:
        // Helper function deep-copying the adjacency lists whose items are references
        // Migrates all the references to the copied this->_nodes and this->_links
        void _deepCopy(const Graph& other) {
            if (other._links.empty()) {
                return;
            }
            auto nodesDiffBytes = reinterpret_cast<const char*>(&_nodes.front())
                    - reinterpret_cast<const char*>(&other._nodes.front());
            auto linksDiffBytes = reinterpret_cast<const char*>(&_links.front())
                    - reinterpret_cast<const char*>(&other._links.front());

            for (auto vp: {&_adjList, &_invAdjList}) {
                for (auto& L: *vp) {
                    auto* pL = reinterpret_cast<std::vector<std::pair<const char*, const char*>>*>(&L);
                    for (auto& item: *pL) {
                        item.first += nodesDiffBytes;
                        item.second += linksDiffBytes;
                    }
                }
            }
        }

    public:
        // Copy constructor. For fast-access adjacency list items,
        //  a _deepCopy() process is required.
        Graph(const Graph& other):
        _nodes(other._nodes), _links(other._links), _adjList(other._adjList),
        _invAdjList(other._invAdjList), _indexMap(other._indexMap) {
            if constexpr (enablesFastRefLink) {
                _deepCopy(other);
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
        void clear()
        requires(strategy == ReserveStrategy::None) {
            _clear();
        }

        void clear(tags::ReservesLater) {
            _clear();
        }

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

    private:
        void _reserve(const ReserveArguments& args) {
            if constexpr(requires {_indexMap.reserve(args);}) {
                _indexMap.reserve(args);
            }

            if (args.nodes != ReserveArguments::notProvided) {
                _nodes.reserve(args.nodes);
                _adjList.resize(args.nodes);
                _invAdjList.resize(args.nodes);
            }

            if (args.links != ReserveArguments::notProvided) {
                _links.reserve(args.links);
            }
        }

    public:
        void reserve(ReserveArguments args) {
            auto s = ReserveStrategy::None;
            if (args.nodes != ReserveArguments::notProvided) {
                s = s | ReserveStrategy::Nodes;
            }
            if (args.links != ReserveArguments::notProvided) {
                s = s | ReserveStrategy::Links;
            }
            if (args.maxIndex != ReserveArguments::notProvided) {
                s = s | ReserveStrategy::MaxIndex;
            }
            assert(s >= (strategy | minimalReserveStrategyOf<IndexMap>()));
            _reserve(args);
        }

        void reserve(const std::map<std::string, std::size_t>& args) {
            reserve(ReserveArguments(args));
        }

        Node* addNode(Node node) {
            return _addNode<tags::DoCheck::Yes>(_indexMap.get(node), std::move(node));
        }

        template <class... Args>
        Node* emplaceNode(Args&&... args) {
            return addNode(Node(std::forward<Args>(args)...));
        }

        Link* addLink(Link link) {
            if (!_checkLink(link)) {
                return nullptr;
            }
            return _addLink<tags::DoCheck::Yes, tags::IsBidirectional::No>(
                    _fastGet1(link), _fastGet2(link), std::move(link)
            );
        }

        template <class... Args>
        Link* emplaceLink(Args&&... args) {
            return addLink(Link(std::forward<Args>(args)...));
        }

        Link* fastAddLink(Link link) {
            return _addLink<tags::DoCheck::No, tags::IsBidirectional::No>(
                    _fastGet1(link), _fastGet2(link), std::move(link)
            );
        }

        template <class... Args>
        Link* fastEmplaceLink(Args&&... args) {
            return fastAddLink(Link(std::forward<Args>(args)...));
        }

        Link* addBiLink(Link link) {
            return _addLink<tags::DoCheck::Yes, tags::IsBidirectional::Yes>(
                    _fastGet1(link), _fastGet2(link), std::move(link)
            );
        }

        template <class... Args>
        Link* emplaceBiLink(Args&&... args) {
            return addBiLink(Link(std::forward<Args>(args)...));
        }

        Link* fastAddBiLink(Link link) {
            return _addLink<tags::DoCheck::No, tags::IsBidirectional::Yes>(
                    _fastGet1(link), _fastGet2(link), std::move(link)
            );
        }

        template <class... Args>
        Link* fastEmplaceBiLink(Args&&... args) {
            return fastAddBiLink(Link(std::forward<Args>(args)...));
        }

        [[nodiscard]] std::size_t nNodes() const {
            return _indexMap.size();
        }

        [[nodiscard]] std::size_t nLinks() const {
            return _links.size();
        }

        bool hasNode(const NodeOrIndex auto& node) const {
            return _indexMap.check(node);
        }

        std::size_t inDegree(const NodeOrIndex auto& to) const {
            return _indexMap.check(to) ? fastInDegree(to) : 0;
        }

        std::size_t fastInDegree(const NodeOrIndex auto& to) const {
            return _invAdjList[_fastGet(to)].size();
        }

        std::size_t outDegree(const NodeOrIndex auto& from) const {
            return _indexMap.check(from) ? fastOutDegree(from) : 0;
        }

        std::size_t fastOutDegree(const NodeOrIndex auto& from) const {
            return _adjList[_fastGet(from)].size();
        }

        Node* node(const NodeOrIndex auto& idx) {
            return hasNode(idx) ? fastNode(idx) : nullptr;
        }

        const Node* node(const NodeOrIndex auto& idx) const {
            return const_cast<Graph*>(this)->node(idx);
        }

        Node* fastNode(const NodeOrIndex auto& idx) {
            return _nodes.data() + _fastGet(idx);
        }

        const Node* fastNode(const NodeOrIndex auto& idx) const {
            return const_cast<Graph*>(this)->fastNode(idx);
        }

        Node& operator [] (const NodeOrIndex auto& idx) {
            return _nodes[_fastGet(idx)];
        }

        const Node& operator [] (const NodeOrIndex auto& idx) const {
            return _nodes[_fastGet(idx)];
        }

        Link* link(const NodeOrIndex auto& u, const NodeOrIndex auto& v) {
            return _indexMap.check(u) && _indexMap.check(v)
                   ? fastLink(u, v)
                   : nullptr;
        }

        const Link* link(const NodeOrIndex auto& u, const NodeOrIndex auto& v) const {
            return const_cast<Graph*>(this)->link(u, v);
        }

        Link* fastLink(const NodeOrIndex auto& u, const NodeOrIndex auto& v) {
            return _findLink(_fastGet(u), _fastGet(v));
        }

        const Link* fastLink(const NodeOrIndex auto& u, const NodeOrIndex auto& v) const {
            return const_cast<Graph*>(this)->fastLink(u, v);
        }

        auto nodes() {
            return std::ranges::subrange(_nodes);
        }

        auto nodes() const {
            return std::ranges::subrange(_nodes);
        }

        auto links() {
            return std::ranges::subrange(_links);
        }

        auto links() const {
            return std::ranges::subrange(_links);
        }

    private:
        enum class Which { From, To };

        template <class ThisPtr>
        static auto _wrap(const AdjList& r, ThisPtr ptr) {
            constexpr bool isConst = std::is_const_v<std::remove_pointer_t<ThisPtr>>;
            if constexpr (enablesFastRefLink) {
                if constexpr (isConst) {
                    return std::ranges::subrange(reinterpret_cast<const ConstAdjList&>(r));
                } else {
                    return std::ranges::subrange(r);
                }
            } else {
                auto func = [ptr](const IndexRefLink& ref) {
                    using ResultType = std::conditional_t<isConst, ConstRefLink, RefLink>;
                    return ResultType(ptr->_nodes[ref.to], ptr->_links[ref.link]);
                };
                return r | std::views::transform(func);
            }
        }

        template <Which which, tags::DoCheck doCheck, class ThisPtr>
        static auto _linksWith(ThisPtr ptr, const NodeOrIndex auto& with) {
            static auto emptyList = AdjList();

            if constexpr (doCheck == tags::DoCheck::Yes) {
                if (!ptr->_indexMap.check(with)) {
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
        auto linksFrom(const NodeOrIndex auto& from) {
            return _linksWith<Which::From, tags::DoCheck::Yes>(this, from);
        }
        auto linksFrom(const NodeOrIndex auto& from) const {
            return _linksWith<Which::From, tags::DoCheck::Yes>(this, from);
        }

        auto fastLinksFrom(const NodeOrIndex auto& from) {
            return _linksWith<Which::From, tags::DoCheck::No>(this, from);
        }
        auto fastLinksFrom(const NodeOrIndex auto& from) const {
            return _linksWith<Which::From, tags::DoCheck::No>(this, from);
        }

        auto linksTo(const NodeOrIndex auto& to) {
            return _linksWith<Which::To, tags::DoCheck::Yes>(this, to);
        }
        auto linksTo(const NodeOrIndex auto& to) const {
            return _linksWith<Which::To, tags::DoCheck::Yes>(this, to);
        }

        auto fastLinksTo(const NodeOrIndex auto& to) {
            return _linksWith<Which::To, tags::DoCheck::No>(this, to);
        }
        auto fastLinksTo(const NodeOrIndex auto& to) const {
            return _linksWith<Which::To, tags::DoCheck::No>(this, to);
        }
    };
}

#endif //DAWNSEEKER_GRAPH_H
