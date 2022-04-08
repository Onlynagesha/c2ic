//
// Created by Onlynagesha on 2022/4/7.
//

#ifndef DAWNSEEKER_GRAPH_GRAPH_BASIC_H
#define DAWNSEEKER_GRAPH_GRAPH_BASIC_H

#include <concepts>
#include "utils/type_traits.h"

namespace graph {
    namespace tags {
        enum class DoCheck {
            Yes, No
        };
        enum class IsBidirectional {
            Yes, No
        };
        enum class IsConst {
            Yes, No
        };

        // Optimizer switch for Graph type
        enum class EnablesFastAccess {
            Yes, No
        };

        // Tag type for Graph type instances that require memory reservation
        struct ReservesLater {
        };
        inline constexpr ReservesLater reservesLater = ReservesLater{};
    }

    // A basic node type that contains simply the index, with index() function provided
    // You can obtain index() for your own node type by inheriting BasicNode
    template <class IndexType>
    class BasicNode {
        IndexType _index;

    public:
        BasicNode() = default;

        explicit BasicNode(IndexType idx) : _index(std::move(idx)) {}

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

        BasicLink(IndexType u, IndexType v) : _from(std::move(u)), _to(std::move(v)) {}

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

    // Identity index for std::basic_string reference
    template <utils::TemplateInstanceOf<std::basic_string> StringType>
    inline decltype(auto) index(StringType&& str) {
        return str;
    }

    // Either node type or index type
    template <class T>
    concept NodeOrIndex = true;

    // Either node type of index type,
    //  with constraint that the index type must be unsigned integer (e.g. std::size_t)
    template <class T>
    concept NodeOrUnsignedIndex = (std::unsigned_integral<std::remove_cvref_t<T>>) || requires(T t) {
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
    concept LinkWithUnsignedIndex = requires(T t) {
        { index1(t) } -> std::unsigned_integral;
        { index2(t) } -> std::unsigned_integral;
    };
}

#endif //DAWNSEEKER_GRAPH_GRAPH_BASIC_H
