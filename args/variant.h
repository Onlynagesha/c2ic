//
// Created by Onlynagesha on 2022/4/5.
//

#ifndef DAWNSEEKER_GRAPH_VARIANT_H
#define DAWNSEEKER_GRAPH_VARIANT_H

#include <ranges>
#include "element.h"

namespace args {
    template<utils::TemplateInstanceOf<std::basic_string> LabelType = std::string>
    class BasicVariant: public BasicInfo<LabelType> {
    private:
        VariantElement  _value;

    public:
        template<class T = std::monostate>
        BasicVariant(LabelType label, // NOLINT(google-explicit-constructor)
                     AlternativeType expectsMask = AlternativeType::All,
                     DescriptionWrapper desc = DescriptionWrapper{},
                     const T &value = std::monostate{}):
                     BasicInfo<LabelType>(label, expectsMask & AlternativeType::All, desc)
        {
            if (maskSizeNoOther(expectsMask) == 0) {
                throw std::invalid_argument("Empty mask is not allowed");
            }
            operator=(value);
        }

        template<class T = std::monostate>
        BasicVariant(std::initializer_list<LabelType> labels,
                     AlternativeType expectsMask =AlternativeType::All,
                     DescriptionWrapper desc = DescriptionWrapper{},
                     const T &value = std::monostate{}):
                     BasicInfo<LabelType>(labels, expectsMask & AlternativeType::All, desc)
        {
            if (maskSizeNoOther(expectsMask) == 0) {
                throw std::invalid_argument("Empty mask is not allowed");
            }
            if (labels.size() == 0) {
                throw std::invalid_argument("Empty label list is not allowed");
            }
            operator=(value);
        }

        template<class T>
        auto &operator=(const T &value) {
            _value = toElement(this->mask(), value);
            return *this;
        }

        template<class T>
        auto get() const {
            return fromElement<T>(_value);
        }

        template<class T>
        auto getOr(const T &alternative) const {
            return fromElementOr(_value, alternative);
        }

        auto i() const {
            return get<std::intmax_t>();
        }

        auto u() const {
            return get<std::uintmax_t>();
        }

        auto f() const {
            return get<long double>();
        }

        auto s() const {
            return get<std::string>();
        }

        auto cis() const {
            return get<utils::ci_string>();
        }

        auto compare(const BasicVariant &rhs) const {
            return compareElements(_value, rhs._value);
        }

        auto compare(const VariantElement &rhs) const {
            return compareElements(_value, rhs);
        }

        template<class T>
        auto compare(const T &rhs) const -> std::partial_ordering {
            return std::visit([&](const auto &lhs) {
                return compareElementValues(lhs, rhs);
            }, _value);
        }

        auto operator <=> (const VariantElement& rhs) const -> std::partial_ordering {
            return compare(rhs);
        }

        auto operator <=> (const BasicVariant& rhs) const -> std::partial_ordering {
            return compare(rhs);
        }

        template <class T>
        auto operator <=> (const T& rhs) const -> std::partial_ordering {
            return compare(rhs);
        }

        template<class T>
        auto &getRef() {
            return refFromElement<T>(_value);
        }

        template<class T>
        const auto &getRef() const {
            return refFromElement<T>(_value);
        }

        [[nodiscard]] auto type() const {
            return std::visit([]<class T>(const auto &) {
                return getElementType<T>();
            }, _value);
        }

        [[nodiscard]] const char *typeName() const {
            return elementTypeName(_value);
        }

        template<utils::StringLike T = std::string>
        [[nodiscard]] auto valueToString() const {
            return utils::toString<T>(_value);
        }

        template <class T = std::string>
        friend auto toString(const BasicVariant& v) {
            auto res = toString<T>(static_cast<const BasicInfo<LabelType>&>(v));
            res.append("\n    Current value:  " + toString<T>(v._value) + " (stored as " + v.typeName() + ")");

            return res;
        }
    };

    using Variant = BasicVariant<std::string>;
    using CIVariant = BasicVariant<utils::ci_string>;
}

#endif //DAWNSEEKER_GRAPH_VARIANT_H
