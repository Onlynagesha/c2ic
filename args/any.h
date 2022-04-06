//
// Created by Onlynagesha on 2022/4/5.
//

#ifndef DAWNSEEKER_GRAPH_ANY_H
#define DAWNSEEKER_GRAPH_ANY_H

#include "exception.h"
#include "variant.h"

namespace args {
    template <utils::TemplateInstanceOf<std::basic_string> LabelType>
    class BasicAny : public BasicInfo<LabelType> {
    private:
        std::variant<VariantElement, std::any> _content;

    public:
        template <class T = std::monostate>
        BasicAny(LabelType label, // NOLINT(google-explicit-constructor)
                 AlternativeType expectsMask = AlternativeType::AllAndOther,
                 DescriptionWrapper desc = DescriptionWrapper{},
                 T value = std::monostate{}):
                 BasicInfo<LabelType>(std::move(label), expectsMask, desc) {
            operator =(std::move(value));
        }

        template <class T = std::monostate>
        BasicAny(std::initializer_list<LabelType> labels,
                 AlternativeType expectsMask = AlternativeType::AllAndOther,
                 DescriptionWrapper desc = DescriptionWrapper{},
                 T value = std::monostate{}):
                 BasicInfo<LabelType>(labels, expectsMask, desc) {
            operator =(std::move(value));
        }

        [[nodiscard]] bool holdsVariant() const {
            return std::holds_alternative<VariantElement>(_content);
        }

        [[nodiscard]] bool holdsOther() const {
            return std::holds_alternative<std::any>(_content);
        }

        template <class T>
        BasicAny& operator =(T value) {
            constexpr auto tv = getElementType<T>();
            if constexpr (isOther(tv)) {
                if (!isNone(this->mask() & tv)) {
                    _content = std::make_any<T>(std::move(value));
                } else {
                    throw BadAnyAccess("Non-variant types are refused by the type mask");
                }
            } else {
                _content = toElement(this->mask(), std::move(value));
            }
            return *this;
        }

        template <class T>
        auto get() const {
            if constexpr (getElementType<T>() != AlternativeType::Other) {
                if (holdsVariant()) {
                    return fromElement<T>(std::get<VariantElement>(_content));
                }
            }
            return std::any_cast<T>(std::get<std::any>(_content));
        }

        template <class T>
        auto getOr(const T& alternative) const try {
            return get<T>();
        } catch (...) {
            return alternative;
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

        template <class T>
        auto compare(const T& rhs) const -> std::partial_ordering {
            if constexpr (getElementType<T>() != AlternativeType::Other) {
                if (holdsVariant()) {
                    return std::visit([&](const auto& lhs) {
                        return compareElementValues(lhs, rhs);
                    }, std::get<VariantElement>(_content));
                } else {
                    throw BadAnyAccess("Comparison fails because current stored is not a variant");
                }
            } else {
                static_assert(utils::AlwaysFalse<T>::value,
                              "Only variant-compatible types can be taken in comparison");
            }
        }

        template <class T>
        auto operator <=>(const T& rhs) const -> std::partial_ordering {
            return compare(rhs);
        }

        template <class T>
        auto& getRef() {
            if constexpr (getElementType<T>() != AlternativeType::None) {
                if (holdsVariant()) {
                    return refFromElement<T>(std::get<VariantElement>(_content));
                }
            }
            return std::any_cast<T&>(std::get<std::any>(_content));
        }

        template <class T>
        const auto& getRef() const {
            return const_cast<BasicAny*>(this)->getRef<T>();
        }

        [[nodiscard]] auto type() const {
            if (holdsVariant()) {
                return getElementType(std::get<VariantElement>(_content));
            } else {
                return AlternativeType::Other;
            }
        }

        [[nodiscard]] decltype(auto) typeName() const {
            if (holdsVariant()) {
                return elementTypeName(std::get<VariantElement>(_content));
            } else {
                return std::get<std::any>(_content).type().name();
            }
        }

        template <utils::StringLike T = std::string>
        [[nodiscard]] auto valueToString() const -> utils::CharTraitsToString<T> {
            if (holdsVariant()) {
                return toString(std::get<VariantElement>(_content));
            } else {
                return "(Not accessible to std::any inside)";
            }
        }

        template <utils::StringLike T = std::string>
        friend auto toString(const BasicAny& v) {
            auto res = toString<T>(static_cast<const BasicInfo<LabelType>&>(v));
            res.append("\n    Current value:  " + v.valueToString() + " (stored as " + v.typeName() + ")");

            return res;
        }
    };
}

#endif //DAWNSEEKER_GRAPH_ANY_H
