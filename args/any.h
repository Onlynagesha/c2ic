//
// Created by Onlynagesha on 2022/4/5.
//

#ifndef DAWNSEEKER_GRAPH_ANY_H
#define DAWNSEEKER_GRAPH_ANY_H

#include "exception.h"
#include "variant.h"

// todo: NOT FINISHED YET. DO NOT USE

namespace args {
    namespace tags {
        struct FromValue {};

        inline FromValue fromValue;
    }

    template <utils::TemplateInstanceOf<std::basic_string> StringType>
    class ArgumentVariantOrAny {
    public:
        using VariantType = BasicVariant<StringType>;

    private:
        std::any        _content;

    public:
        ArgumentVariantOrAny() = default;

        ArgumentVariantOrAny(VariantType value) { // NOLINT(google-explicit-constructor)
            _content = std::move(value);
        }

        template <class T>
        ArgumentVariantOrAny(tags::FromValue, T value) {
            static_assert(isInvalid(getElementType<T>()),
                          "Assigning an arg. any from a variable compatible to BasicVariant is disallowed");

            _content = std::move(value);
        }

        template<class T = std::monostate>
        ArgumentVariantOrAny(const char* label, // NOLINT(google-explicit-constructor)
                             AlternativeType expectsMask = AlternativeType::All,
                             DescriptionWrapper desc = DescriptionWrapper{},
                             const T &value = std::monostate{}) {
            _content = VariantType(label, expectsMask, desc, value);
        }

        template<class T = std::monostate>
        ArgumentVariantOrAny(std::initializer_list<StringType> labels,
                             AlternativeType expectsMask = AlternativeType::All,
                             DescriptionWrapper desc = DescriptionWrapper{},
                             const T &value = std::monostate{}) {
            _content = VariantType(labels, expectsMask, desc, value);
        }

        [[nodiscard]] bool holdsVariant() const {
            return _content.type() == typeid(VariantType);
        }

        template <class T>
        ArgumentVariantOrAny& operator = (T value) {
            constexpr auto tv = getElementType<T>();
            if constexpr (isInvalid(tv)) {
                _content = std::move(value);
            } else if (holdsVariant()) {
                std::any_cast<VariantType&>(_content) = std::move(value);
            } else {
                constexpr auto what = "The any type does not hold a variant, "
                                      "so attempting to assign a value to a variant is invalid";
                throw BadArgumentAnyAccess(what);
            }
            return *this;
        }

        template<class T> auto get() const {
            constexpr auto tv = getElementType<T>();
            if constexpr (isInvalid(tv)) {
                return std::any_cast<const T&>(_content);
            } else if (holdsVariant()) {
                return std::any_cast<const VariantType&>(_content).template get<T>();
            } else {
                constexpr auto what = "The any type does not hold a variant, "
                                      "so attempting to get a value from a variant is invalid";
                throw BadArgumentAnyAccess(what);
            }
        }

        template <class T> auto getOr(const T& alternative) const try {
            return get<T>();
        } catch (...) {
            return alternative;
        }

        template <class T> auto compare(const T& rhs) const -> std::partial_ordering {
            if (_content.type() == typeid(VariantType)) {
                return std::any_cast<const VariantType&>(_content).compare(rhs);
            } else {
                constexpr auto what = "The any type does not hold a variant, "
                                      "so comparison is not allowed";
                throw BadArgumentAnyAccess(what);
            }
        }

        template <class T> auto operator <=> (const T& rhs) const -> std::partial_ordering {
            return compare(rhs);
        }

        template <class T> auto& getRef() {
            constexpr auto tv = getElementType<T>();
            if constexpr (isInvalid(tv)) {
                return std::any_cast<T&>(_content);
            } else if (holdsVariant()) {
                return std::any_cast<VariantType&>(_content).template getRef<T>();
            } else {
                constexpr auto what = "The any type does not hold a variant, "
                                      "so attempting to get a reference is invalid";
                throw BadArgumentAnyAccess(what);
            }
        }

        template <class T> const auto& getRef() const {
            return const_cast<ArgumentVariantOrAny*>(this)->getRef<T>();
        }

        bool matches(const utils::StringLike auto& label) const {
            if (holdsVariant()) {
                return std::any_cast<const VariantType&>(_content).matches(label);
            } else {
                constexpr auto what = "The any type does not hold a variant, "
                                      "so attempting to check a label is invalid";
                throw BadArgumentAnyAccess(what);
            }
        }

        [[nodiscard]] decltype(auto) labels() const {
            if (holdsVariant()) {
                return std::any_cast<const VariantType&>(_content).labels();
            } else {
                constexpr auto what = "The any type does not hold a variant, "
                                      "so attempting to get the label list is invalid";
                throw BadArgumentAnyAccess(what);
            }
        }

        [[nodiscard]] decltype(auto) description() const {
            if (holdsVariant()) {
                return std::any_cast<const VariantType&>(_content).description();
            } else {
                constexpr auto what = "The any type does not hold a variant, "
                                      "so attempting to get the description is invalid";
                throw BadArgumentAnyAccess(what);
            }
        }

        [[nodiscard]] auto type() const {
            if (holdsVariant()) {
                return std::any_cast<const VariantType&>(_content).type();
            } else {
                constexpr auto what = "The any type does not hold a variant, "
                                      "so attempting to get the type flag is invalid";
                throw BadArgumentAnyAccess(what);
            }
        }

        [[nodiscard]] decltype(auto) typeName() const {
            if (holdsVariant()) {
                return std::any_cast<const VariantType&>(_content).typeName();
            } else {
                constexpr auto what = "The any type does not hold a variant, "
                                      "so attempting to get the type name is invalid";
                throw BadArgumentAnyAccess(what);
            }
        }

        template<utils::StringLike T = std::string>
        [[nodiscard]] auto valueToString() const {
            if (holdsVariant()) {
                return std::any_cast<const VariantType&>(_content).template valueToString<T>();
            } else {
                constexpr auto what = "The any type does not hold a variant, "
                                      "so attempting to get the value as string is invalid";
                throw BadArgumentAnyAccess(what);
            }
        }

        template<utils::StringLike T = std::string>
        friend auto toString(const ArgumentVariantOrAny& value) {
            if (value.holdsVariant()) {
                return toString<T>(std::any_cast<const VariantType&>(value._content));
            } else {
                return utils::CharTraitsToString<T>{"(Value of other types)"};
            }
        }
    };
}

#endif //DAWNSEEKER_GRAPH_ANY_H
