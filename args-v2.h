//
// Created by dawnseeker on 2022/4/1.
//

#ifndef DAWNSEEKER_GRAPH_ARGS_V2_H
#define DAWNSEEKER_GRAPH_ARGS_V2_H

#include <charconv>
#include <compare>
#include <concepts>
#include <cstring>
#include <functional>
#include <map>
#include <memory>
#include <ranges>
#include <string>
#include <utility>
#include <variant>
#include <vector>
#include "global.h"
#include "immbasic.h"
#include "utils.h"

using ArgumentVariantElement = std::variant<
        std::monostate,         // None yet
        std::intmax_t,          // Signed integer
        std::uintmax_t,         // Unsigned integer
        double,                 // Floating point
        std::string,            // String
        ci_string               // String, but case-insensitive
        >;

class BadArgumentVariantAccess: std::bad_variant_access {
    const char* _what = "BadArgumentVariantAccess";

public:
    BadArgumentVariantAccess() = default;
    explicit BadArgumentVariantAccess(const char* info): std::bad_variant_access(), _what(info) {}

    [[nodiscard]] const char* what() const noexcept override {
        return _what;
    }
};

template <class T>
inline auto toArgumentVariantElementValue(const T& value) {
    if constexpr (std::is_same_v<T, std::monostate>) {
        return std::monostate{};
    } else if constexpr (std::is_signed_v<T>) {
        return std::intmax_t{value};
    } else if constexpr (std::is_unsigned_v<T>) {
        return std::uintmax_t{value};
    } else if constexpr (std::is_floating_point_v<T>) {
        return double{value};
    } else if constexpr (std::is_same_v<T, ci_string>) {
        return value;
    } else if constexpr (StringLike<T>) {
        return std::string{value};
    } else {
        static_assert(AlwaysFalse<T>::value,
                      "Only integers, floating-points, and strings "
                      "(either case sensitive or insensitive) are supported!");
    }
}

inline auto toArgumentVariantElementValue() {
    return std::monostate{};
}

template <class U, SameAsOneOfWithoutCVRef<ArgumentVariantElement> T>
inline auto fromArgumentVariantElementValue(const T& value) {
    // std::monostate: always throws an exception
    if constexpr (std::is_same_v<T, std::monostate>) {
        throw BadArgumentVariantAccess("Visits std::monostate (i.e. no value yet)");
    }
    // Converts from an arithmetic value
    else if constexpr (std::is_arithmetic_v<T>) {
        if constexpr (TemplateInstanceOf<U, std::basic_string>) {
            // String <- Arithmetic, where String must be an instance of std::basic_string
            // (in other words, const char* is not supported)
            return toString(value);
        } else if constexpr (std::is_arithmetic_v<U>) {
            // Arithmetic <- Arithmetic
            // Exception may be thrown if underflow or overflow happens
            return value_safe_arithmetic_cast<U>(value);
        } else {
            // Destination type U is invalid (e.g. const char*)
            static_assert(AlwaysFalse<U>::value, "U must be either arithmetic, or an instance of std::basic_string");
        }
    }
    // Converts from a string
    else if constexpr (StringLike<T>) {
        if constexpr (TemplateInstanceOf<U, std::basic_string>) {
            // String <- String
            return string_traits_cast<U>(value);
        } else if constexpr (std::is_arithmetic_v<U>) {
            // Arithmetic <- String
            return fromString<U>(value);
        } else {
            // Destination type U is invalid (e.g. const char*)
            static_assert(AlwaysFalse<U>::value, "U must be either arithmetic, or an instance of std::basic_string");
        }
    }
    // Unsupported T
    else {
        static_assert(AlwaysFalse<T>::value, "Unimplemented or invalid type T as input");
    }
}

template <class U, SameAsOneOf<ArgumentVariantElement> T>
inline auto fromArgumentVariantElementValueOr(const T& value, const U& alternative) try {
    return fromArgumentVariantElementValue<U>(value);
} catch (...) {
    return alternative;
}

template <SameAsOneOf<ArgumentVariantElement> T, SameAsOneOf<ArgumentVariantElement> U>
inline auto compareArgumentVariantElements(const T& A, const U& B) -> std::partial_ordering {
    // First check std::monostate (i.e. no value)
    // No-value is regarded the lowest
    constexpr auto mT = (int) std::is_same_v<T, std::monostate>;
    constexpr auto mU = (int) std::is_same_v<U, std::monostate>;
    if constexpr (mT || mU) {
        return mT <=> mU;
    }
    // Then check string, two strings are compared as case-insensitive of either is ci_string
    // Otherwise, compare case-sensitively
    else if constexpr (StringLike<T> && StringLike<U>) {
        if constexpr (std::same_as<T, ci_string> || std::same_as<U, ci_string>) {
            return ci_strcmp(std::ranges::cdata(A), std::ranges::cdata(B));
        } else {
            return A <=> B;
        }
    }
    // Then for arithmetic types, two numbers are compared by their values
    //  with (possibly) implicit conversion
    else if constexpr (std::is_arithmetic_v<T> && std::is_arithmetic_v<U>) {
        if constexpr (std::is_integral_v<T> && std::is_integral_v<U>) {
            // If both are integers, compares with the value-safe functions
            return std::cmp_equal(A, B) ? std::strong_ordering::equal :
                   std::cmp_less(A, B) ? std::strong_ordering::less : std::strong_ordering::greater;
        } else {
            // Otherwise, just let they get converted implicitly
            return A <=> B;
        }
    }
    // Then for arithmetic-string mixed comparison
    // Transforms the string to the arithmetic type. an exception may be thrown for conversion failure
    // 1. (String, Arithmetic)
    else if constexpr (StringLike<T>) {
        return fromString<U>(A) <=> B;
    }
    // 2. (Arithmetic, String)
    else if constexpr (StringLike<U>) {
        return A <=> fromString<T>(B);
    }
    // Unsupported cases
    else {
        static_assert(AlwaysFalse<T>::value, "Unsupported or invalid types");
    }
}

inline std::partial_ordering compareArgumentVariants(
        const ArgumentVariantElement& A, const ArgumentVariantElement& B) {
    const auto cmp = [](const auto& a, const auto& b) {
        return compareArgumentVariantElements(a, b);
    };
    return std::visit(cmp, A, B);
}

namespace ArgumentVariantLiterals {
    inline consteval std::size_t operator ""_expects(const char* str, std::size_t len) {
        auto equalsToEither = [](const char* str, std::convertible_to<const char*> auto... args) constexpr {
            return ((ci_strcmp(str, args) == std::strong_ordering::equal) || ...);
        };

        auto mask = std::size_t{1}; // std::monostate is always allowed
        splitByEither(str, len, ",; |", [&](const char* str, std::size_t) constexpr {
            if (equalsToEither(str, "all")) {
                mask = 0b11'11'11u;
            } else if (equalsToEither(str, "i", "d", "int", "signed", "intmax_t", "std::intmax_t")) {
                mask |= (1u << 1);
            } else if (equalsToEither(str, "u", "unsigned", "uintmax_t", "std::uintmax_t")) {
                mask |= (1u << 2);
            } else if (equalsToEither(str, "f", "float", "double", "floating")) {
                mask |= (1u << 3);
            } else if (equalsToEither(str, "s", "str", "string", "std::string")) {
                mask |= (1u << 4);
            } else if (equalsToEither(str, "cis", "ci_str", "cistr", "ci_string", "cistring")) {
                mask |= (1u << 5);
            }
        });
        return mask;
    }
}

// Returns a copy of the holding value. Type conversion may be performed.
template <class T_>
inline auto getArgumentVariantElementValue(const ArgumentVariantElement& v) {
    using T = std::remove_cvref_t<T_>;
    return std::visit([](const auto& value) {
        return fromArgumentVariantElementValue<T>(value);
    }, v);
}

// Returns a non-const reference of the holding value
template <class T_>
inline auto& getArgumentVariantElementReference(ArgumentVariantElement& v) {
    using T = std::remove_cvref_t<T_>;
    return std::get<T>(v);
}

// Returns a const reference to the holding value
template <class T_>
inline const auto& getArgumentVariantElementReference(const ArgumentVariantElement& v) {
    return getArgumentVariantElementReference<T_>(const_cast<ArgumentVariantElement&>(v));
}

template <class TraitsOrStr = std::string>
inline auto getArgumentVariantElementValueAsString(const ArgumentVariantElement& v) {
    using Str = CharTraitsToString<TraitsOrStr>;
    return std::visit([]<class T>(const T& x) -> Str {
        if constexpr (std::is_same_v<std::monostate, std::remove_cvref_t<T>>) {
            return "(No value yet)";
        } else {
            return fromArgumentVariantElementValue<Str>(x);
        }
    }, v);
}

template <class T>
inline constexpr const char* argumentVariantElementTypeName() {
    if constexpr (!std::is_same_v<T, std::remove_cvref_t<T>>) {
        return argumentVariantElementTypeName<std::remove_cvref_t<T>>();
    } else {
        if constexpr (std::same_as<T, std::monostate>) {
            return "std::monostate";
        } else if constexpr (std::integral<T>) {
            return integralTypeName<T>();
        } else if constexpr (std::same_as<T, double>) {
            return "double";
        } else if constexpr (std::same_as<T, std::string>) {
            return "std::string";
        } else if constexpr (std::same_as<T, ci_string>) {
            return "ci_string";
        } else {
            static_assert(AlwaysFalse<T>::value, "Unimplemented or invalid element type");
        }
    }
}

inline constexpr const char* argumentVariantElementTypeName(const ArgumentVariantElement& v) {
    return std::visit([]<class T>(const T&) { return argumentVariantElementTypeName<T>(); }, v);
}

template <class StringType = std::string>
class ArgumentVariant {
public:
    static constexpr std::size_t allMask = (std::size_t{1} << std::variant_size_v<ArgumentVariantElement>) - 1;

private:
    using LabelListType = std::vector<StringType>;

    std::size_t             _expectsMask{};
    LabelListType           _labels;
    ArgumentVariantElement  _value;

public:
    explicit ArgumentVariant(StringType label):
    ArgumentVariant(allMask, std::move(label), std::monostate{}) {}

    ArgumentVariant(std::initializer_list<StringType> labels):
    ArgumentVariant(allMask, labels, std::monostate{}) {}

    ArgumentVariant(std::size_t expectsMask, StringType label):
    ArgumentVariant(expectsMask, std::move(label), std::monostate{}) {}

    ArgumentVariant(std::size_t expectsMask, std::initializer_list<StringType> labels):
    ArgumentVariant(expectsMask, labels, std::monostate{}) {}

    template <class T>
    ArgumentVariant(StringType label, const T& value):
    ArgumentVariant(allMask, std::move(label), value) {}

    template <class T>
    ArgumentVariant(std::initializer_list<StringType> labels, const T& value):
    ArgumentVariant(allMask, labels, value) {}

    template <class T>
    ArgumentVariant(std::size_t expectsMask, StringType label, const T& value):
    _expectsMask(expectsMask), _labels({std::move(label)}), _value(toArgumentVariantElementValue(value)) {}

    template <class T>
    ArgumentVariant(std::size_t expectsMask, std::initializer_list<StringType> labels, const T& value):
    _expectsMask(expectsMask), _labels(labels), _value(toArgumentVariantElementValue(value)) {}

    [[nodiscard]] const auto& labels() const {
        return _labels;
    }

    template <StringLike T>
    bool matches(const T& label) const {
        return rs::find(_labels, label) != rs::end(_labels);
    }

    template <class T>
    auto get() const {
        return getArgumentVariantElementValue<T>(_value);
    }

    template <class T>
    auto getOr(const T& alternative) const {
        try {
            auto res = get<T>();
            return res;
        } catch (std::bad_variant_access&) {
            return alternative;
        } catch (...) {
            throw;
        }
    }

    template <class T>
    auto& getRef() {
        return getArgumentVariantElementReference<T>(_value);
    }

    template <class T>
    const auto& getRef() const {
        return getArgumentVariantElementReference<T>(_value);
    }

    [[nodiscard]] const char* typeName() const {
        return argumentVariantElementTypeName(_value);
    }

    template <class T = std::string>
    [[nodiscard]] auto toString() const {
        return getArgumentVariantElementValueAsString<T>(_value);
    }
};

template <class StringType = std::string>
class ArgumentVariantSet {
    using VariantType = ArgumentVariant<StringType>;
    using MapKey = StringType;
    using MapValue = std::shared_ptr<VariantType>;

    std::map<MapKey, MapValue, std::ranges::less> _variants;

public:
    ArgumentVariantSet() = default;
    ArgumentVariantSet(std::initializer_list<VariantType> variants) {
        for (const auto& v: variants) {
            add(v);
        }
    }

    void clear() {
        _variants.clear();
    }

    void add(VariantType v) {
        auto p = std::make_shared<VariantType>(std::move(v));
        for (const auto& str: p->labels()) {
            assert(!_variants.contains(str));
            _variants[str] = p;
        }
    }

    auto get(const StringLike auto& label) const -> MapValue {
        auto it = _variants.find(label);
        return it == _variants.end() ? nullptr : it->second;
    }

    auto operator [] (const StringLike auto& label) const {
        return get(label);
    }
};

struct ProgramArguments {
    // Path of the graph file
    std::string             graphPath;
    // Path of the seed set file
    std::string             seedSetPath;
    // Which algorithm to use
    // "auto": decided by node state priority
    // "pr-imm": PR-IMM algorithm (for M-S)
    // "sa-imm": SA-IMM algorithm (for M-nS)
    // "sa-rg-imm": SA-RG-IMM algorithm (for nM-nS)
    ci_string   algo;
    // Priority values of all the node states (Ca+, Ca, Cr, Cr-, and None)
    NodeStatePriorityArray  priority;
    // Controls the weights that
    //  positive message and negative message contribute respectively
    // See nodeStateGain[] and setNodeStateGain(lambda) for details
    double          lambda;
    // Number of boosted nodes to choose
    std::size_t     k;
    // The maximum number of PRR-sketch samples
    std::size_t     sampleLimit;
    // The maximum number of PRR-sketches per center node v in SA-IMM algorithm
    std::size_t     sampleLimitSA;
};

#endif //DAWNSEEKER_GRAPH_ARGS_V2_H
