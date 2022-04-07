//
// Created by Onlynagesha on 2022/4/7.
//

#ifndef DAWNSEEKER_GRAPH_INDEXMAP_H
#define DAWNSEEKER_GRAPH_INDEXMAP_H

#include <algorithm>
#include <cassert>
#include <iostream>
#include <map>
#include <vector>
#include "basic.h"

namespace graph {
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
     * .reserve(args) performs memory pre-allocation, with args: std::map<std::string, std::size_t>
     * .clear() clears the index map to initial state
     * .reserveClear() clears the index map, with the last reservation still works,
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
    class LinearIndexMap {
    private:
        // Null index, _map[v] == null indicates that node v has not been allocated a mapped index
        static constexpr std::size_t null = static_cast<std::size_t>(-1);
        // Linear list as the index map
        std::vector<std::size_t> _map;

    public:
        LinearIndexMap() = default;

        // Reserves the linear list by maximal original index
        void reserve(const std::map<std::string, std::size_t>& args) {
            auto it = args.find("maxIndex");
            if (it == args.end()) {
                std::cerr << "WARNING on LinearIndexMap::reserve: argument 'maxIndex' is not provided!"
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

        // Resets the index map, with _map still reserved by the maxIndex
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
            return const_cast<LinearIndexMap*>(this)->_get(index(node));
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

    // Stores mapped indices with an associative container
    // The most generic one (as long as IndexType supports operator <),
    //  but with the cost of O(log |V|) lookup
    template <class Node>
    class AssociativeIndexMap {
    public:
        using IndexType = std::remove_cvref_t<decltype(index(std::declval<Node>()))>;

    private:
        std::map<IndexType, std::size_t> _map;

    public:
        AssociativeIndexMap() = default;

        void clear() {
            _map.clear();
        }

        std::size_t get(const NodeOrIndex auto& node) const {
            auto it = _map.find(index(node));
            assert(it != _map.end());
            return it->second;
        }

        bool check(const NodeOrIndex auto& node) const {
            return _map.contains(index(node));
        }

        void set(const NodeOrIndex auto& node, std::size_t mappedIndex) {
            _map[index(node)] = mappedIndex;
        }
    };

}

#endif //DAWNSEEKER_GRAPH_INDEXMAP_H
