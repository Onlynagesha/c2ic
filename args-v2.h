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

namespace args {
    struct DescriptionWrapper {
        const char *str;
        const char *endPos;

        [[nodiscard]] constexpr std::size_t length() const {
            return static_cast<std::size_t>(endPos - str);
        }

        constexpr operator const char *() const { // NOLINT(google-explicit-constructor)
            return str;
        }
    };

    enum class AlternativeType : unsigned {
        None = 0,
        SignedInteger = 1,
        UnsignedInteger = 2,
        AnyInteger = 3,
        FloatingPoint = 4,
        CaseSensitiveString = 8,
        CaseInsensitiveString = 16,
        AnyString = 24,
        All = 31,
    };

    constexpr inline auto &operator|=(AlternativeType &L, AlternativeType R) {
        unsigned value = (unsigned) L | (unsigned) R;
        L = (AlternativeType) value;
        return L;
    }

    constexpr inline auto operator|(AlternativeType L, AlternativeType R) {
        L |= R;
        return L;
    }

    constexpr inline auto &operator&=(AlternativeType &L, AlternativeType &R) {
        unsigned value = (unsigned) L & (unsigned) R;
        L = (AlternativeType) value;
        return L;
    }

    constexpr inline auto operator&(AlternativeType L, AlternativeType R) {
        L &= R;
        return L;
    }

    constexpr inline bool isNone(AlternativeType A) {
        return A == AlternativeType::None;
    }

    template <class T = std::string>
    inline auto toString(AlternativeType A) {
        using ResultType = CharTraitsToString<T>;

        if (A == AlternativeType::None) {
            return ResultType{"None"};
        }

        auto res = CharTraitsToString<T>{};
        res.reserve(128);

        auto append = [&](const char* content) {
            if (! res.empty()) {
                res += " | ";
            }
            res += content;
        };

        if (!isNone(A & AlternativeType::SignedInteger)) {
            append("Signed integer");
        }
        if (!isNone(A & AlternativeType::UnsignedInteger)) {
            append("Unsigned integer");
        }
        if (!isNone(A & AlternativeType::FloatingPoint)) {
            append("Floating point");
        }
        if (!isNone(A & AlternativeType::CaseSensitiveString)) {
            append("Case-sensitive string");
        }
        if (!isNone(A & AlternativeType::CaseInsensitiveString)) {
            append("Case-insensitive string");
        }
        return res;
    }

    using VariantElement = std::variant<
            std::monostate,         // None yet
            std::intmax_t,          // Signed integer
            std::uintmax_t,         // Unsigned integer
            long double,            // Floating point
            std::string,            // String
            ci_string               // String, but case-insensitive
    >;

    template<class T_>
    inline constexpr auto getElementType() {
        using T = std::remove_cvref_t<T_>;

        if constexpr (std::is_same_v<T, std::monostate>) {
            return AlternativeType::None;
        } else if constexpr (std::is_signed_v<T>) {
            return AlternativeType::SignedInteger;
        } else if constexpr (std::is_unsigned_v<T>) {
            return AlternativeType::UnsignedInteger;
        } else if constexpr (std::is_floating_point_v<T>) {
            return AlternativeType::FloatingPoint;
        } else if constexpr (std::is_same_v<T, ci_string>) {
            return AlternativeType::CaseInsensitiveString;
        } else if constexpr (StringLike<T>) {
            return AlternativeType::CaseSensitiveString;
        } else {
            static_assert(AlwaysFalse<T>::value, "Invalid type T");
        }
    }

    inline auto getElementType(const VariantElement &v) {
        return std::visit([]<class T>(const T &) {
            return getElementType<T>();
        }, v);
    }

    inline auto getElementType(std::size_t index) {
        if (index == std::variant_npos) {
            return AlternativeType::None;
        } else {
            return static_cast<AlternativeType>(1u << index);
        }
    }

    class BadArgumentVariantAccess : std::bad_variant_access {
        const char *_what = "BadArgumentVariantAccess";

    public:
        BadArgumentVariantAccess() = default;

        explicit BadArgumentVariantAccess(const char *info) : std::bad_variant_access(), _what(info) {}

        [[nodiscard]] const char *what() const noexcept override {
            return _what;
        }
    };

    template<class T>
    inline auto toElementValue(const T &value) {
        constexpr auto tv = getElementType<T>();

        if constexpr (tv == AlternativeType::None) {
            return std::monostate{};
        } else if constexpr (tv == AlternativeType::SignedInteger) {
            return std::intmax_t{value};
        } else if constexpr (tv == AlternativeType::UnsignedInteger) {
            return std::uintmax_t{value};
        } else if constexpr (tv == AlternativeType::FloatingPoint) {
            return (long double)value;
        } else if constexpr (tv == AlternativeType::CaseInsensitiveString) {
            return value;
        } else if constexpr (tv == AlternativeType::CaseSensitiveString) {
            return std::string{toCString(value)};
        } else {
            static_assert(AlwaysFalse<T>::value,
                          "Only integers, floating-points, and strings "
                          "(either case sensitive or insensitive) are supported!");
        }
    }

    inline auto toElementValue() {
        return std::monostate{};
    }

    template<class U, SameAsOneOfWithoutCVRef<VariantElement> T>
    inline U fromElementValue(const T &value) {
        // std::monostate: always throws an exception
        if constexpr (std::is_same_v<T, std::monostate>) {
            throw BadArgumentVariantAccess("Visits std::monostate (i.e. no value yet)");
        }
            // Converts from an arithmetic value
        else if constexpr (std::is_arithmetic_v<T>) {
            if constexpr (TemplateInstanceOf<U, std::basic_string>) {
                // String <- Arithmetic, where String must be an instance of std::basic_string
                // (in other words, const char* is not supported)
                return ::toString<U>(value);
            } else if constexpr (std::is_arithmetic_v<U>) {
                // Arithmetic <- Arithmetic
                // Exception may be thrown if underflow or overflow happens
                return value_safe_arithmetic_cast<U>(value);
            } else {
                // Destination type U is invalid (e.g. const char*)
                static_assert(AlwaysFalse<U>::value,
                              "U must be either arithmetic, or an instance of std::basic_string");
            }
        }
            // Converts from a string
        else if constexpr (StringLike<T>) {
            if constexpr (TemplateInstanceOf<U, std::basic_string>) {
                // String <- String
                return string_traits_cast<U>(value);
            } else if constexpr (std::is_arithmetic_v<U>) {
                // Arithmetic <- String (Exception may be throw if fails
                return ::fromString<U>(value);
            } else {
                // Destination type U is invalid (e.g. const char*)
                static_assert(AlwaysFalse<U>::value,
                              "U must be either arithmetic, or an instance of std::basic_string");
            }
        }
            // Unsupported T
        else {
            static_assert(AlwaysFalse<T>::value, "Unimplemented or invalid type T as input");
        }
    }

    template<class U, SameAsOneOf<VariantElement> T>
    inline auto fromElementValueOr(const T &value, const U &alternative) try {
        return fromElementValue<U>(value);
    } catch (...) {
        return alternative;
    }

    template<SameAsOneOf<VariantElement> T, SameAsOneOf<VariantElement> U>
    inline auto compareElementValues(const T &A, const U &B) -> std::partial_ordering {
        // First check std::monostate (i.e. no value)
        // No-value is regarded the lowest
        constexpr auto mT = (int) std::is_same_v<T, std::monostate>;
        constexpr auto mU = (int) std::is_same_v<U, std::monostate>;
        if constexpr (mT || mU) {
            return (1 - mT) <=> (1 - mU);
        }
            // Then check string, two strings are compared as case-insensitive of either is ci_string
            // Otherwise, compare case-sensitively
        else if constexpr (StringLike<T> && StringLike<U>) {
            if constexpr (std::same_as<T, ci_string> || std::same_as<U, ci_string>) {
                return cstr::ci_strcmp(std::ranges::cdata(A), std::ranges::cdata(B));
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

    inline std::partial_ordering compareElements(
            const VariantElement &A, const VariantElement &B) {
        const auto cmp = [](const auto &a, const auto &b) {
            return compareElementValues(a, b);
        };
        return std::visit(cmp, A, B);
    }

    namespace literals {
        inline constexpr auto operator ""_expects(const char *typesStr, std::size_t len) {
            auto equalsToEither = [](const char *str, std::size_t n,
                    std::convertible_to<const char *> auto... args) constexpr {
                return ((n == cstr::strlen(args) &&
                cstr::ci_strncmp(str, args, n) == std::strong_ordering::equal) || ...);
            };

            auto mask = AlternativeType::None;
            splitByEither(typesStr, len, ", |", [&](const char *str, std::size_t n) constexpr {
                if (equalsToEither(str, n, "all")) {
                    mask = AlternativeType::All;
                } else if (equalsToEither(str, n, "i", "d", "int", "signed", "intmax_t", "std::intmax_t")) {
                    mask |= AlternativeType::SignedInteger;
                } else if (equalsToEither(str, n, "u", "unsigned", "uintmax_t", "std::uintmax_t")) {
                    mask |= AlternativeType::UnsignedInteger;
                } else if (equalsToEither(str, n, "f", "float", "double", "floating")) {
                    mask |= AlternativeType::FloatingPoint;
                } else if (equalsToEither(str, n, "s", "str", "string", "std::string")) {
                    mask |= AlternativeType::CaseSensitiveString;
                } else if (equalsToEither(str, n, "cis", "ci_str", "cistr", "ci_string", "cistring")) {
                    mask |= AlternativeType::CaseInsensitiveString;
                }
            });
            if (mask == AlternativeType::None) {
                throw "Invalid argument: At least one type shall be supported! " // NOLINT(hicpp-exception-baseclass)
                      "(valid tokens see above)";
            }
            return mask;
        }

        inline constexpr auto operator ""_desc(const char *str, std::size_t len) {
            return DescriptionWrapper{str, str + len};
        }
    }

// Returns a copy of the holding value. Type conversion may be performed.
    template<class T_>
    inline auto fromElement(const VariantElement &v) {
        using T = std::remove_cvref_t<T_>;

        static std::byte buffer[sizeof(T)];

        std::visit([](const auto &value) {
            *(reinterpret_cast<T*>(buffer)) = fromElementValue<T>(value);
        }, v);

        return std::move(*(reinterpret_cast<T*>(buffer)));
    }

    template<class T>
    inline auto fromElementOr(const VariantElement &v, const T &alternative) {
        static std::byte buffer[sizeof(T)];
        std::visit([&](const auto &value) {
            *(reinterpret_cast<T*>(buffer)) = fromElementValueOr(value, alternative);
        }, v);
        return std::move(*(reinterpret_cast<T*>(buffer)));
    }

// Returns a non-const reference of the holding value
    template<class T_>
    inline auto &refFromElement(VariantElement &v) {
        using T = std::remove_cvref_t<T_>;
        return std::get<T>(v);
    }

// Returns a const reference of the holding value
    template<class T_>
    inline const auto &crefFromElement(VariantElement &v) {
        using T = std::remove_cvref_t<T_>;
        return std::get<T>(v);
    }

// Returns a const reference to the holding value
    template<class T_>
    inline const auto &refFromElement(const VariantElement &v) {
        return crefFromElement<T_>(const_cast<VariantElement &>(v));
    }

// Returns a const reference to the holding value
    template<class T_>
    inline const auto &crefFromElement(const VariantElement &v) {
        return refFromElement<T_>(v);
    }

    template<class TraitsOrStr = std::string>
    inline auto toString(const VariantElement &v) {
        using Str = CharTraitsToString<TraitsOrStr>;
        return std::visit([]<class T>(const T &x) -> Str {
            if constexpr (std::is_same_v<std::monostate, std::remove_cvref_t<T>>) {
                return "(No value yet)";
            } else {
                return fromElementValue<Str>(x);
            }
        }, v);
    }

    template<class T>
    inline constexpr const char *elementTypeName() {
        if constexpr (!std::is_same_v<T, std::remove_cvref_t<T>>) {
            return elementTypeName<std::remove_cvref_t<T>>();
        } else {
            if constexpr (std::same_as<T, std::monostate>) {
                return "std::monostate";
            } else if constexpr (std::integral<T>) {
                return integralTypeName<T>();
            } else if constexpr (std::same_as<T, long double>) {
                return "long double";
            } else if constexpr (std::same_as<T, std::string>) {
                return "std::string";
            } else if constexpr (std::same_as<T, ci_string>) {
                return "std::basic_string<char, utils::ci_char_traits>";
            } else {
                static_assert(AlwaysFalse<T>::value, "Unimplemented or invalid element type");
            }
        }
    }

    inline constexpr const char *elementTypeName(const VariantElement &v) {
        return std::visit([]<class T>(const T &) { return elementTypeName<T>(); }, v);
    }

    template<TemplateInstanceOf<std::basic_string> StringType = std::string>
    class ArgumentVariant {
    public:
        static constexpr auto allMask = AlternativeType::All;

    private:
        using LabelListType = std::vector<StringType>;

        LabelListType   _labels;
        AlternativeType _expectsMask;
        StringType      _description;
        VariantElement  _value;

    public:
        template<class T = std::monostate>
        ArgumentVariant(StringType label,
                        AlternativeType expectsMask,
                        DescriptionWrapper desc,
                        const T &value = std::monostate{}):
                _labels({std::move(label)}),
                _expectsMask(expectsMask),
                _description(desc.str, desc.endPos) {
            operator=(value);
        }

        template<class T = std::monostate>
        ArgumentVariant(std::initializer_list<StringType> labels,
                        AlternativeType expectsMask,
                        DescriptionWrapper desc,
                        const T &value = std::monostate{}):
                _labels(labels),
                _expectsMask(expectsMask),
                _description(desc.str, desc.endPos) {
            if (labels.size() == 0) {
                throw std::invalid_argument("Empty label list is not allowed");
            }
            operator=(value);
        }

        template<class T>
        auto &operator=(const T &value) {
            auto tv = getElementType<T>();
            if (!isNone(tv) && isNone(_expectsMask & tv)) {
                throw std::invalid_argument("Bad assignment to ArgumentVariant: unexpected type");
            }
            _value = toElementValue(value);
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

        auto compare(const ArgumentVariant &rhs) const {
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

        template<class T>
        auto &getRef() {
            return refFromElement<T>(_value);
        }

        template<class T>
        const auto &getRef() const {
            return refFromElement<T>(_value);
        }

        template<StringLike T>
        bool matches(const T &label) const {
            return rs::find(_labels, label) != rs::end(_labels);
        }

        [[nodiscard]] const auto &labels() const {
            return _labels;
        }

        [[nodiscard]] const auto &description() const {
            return _description;
        }

        [[nodiscard]] auto type() const {
            return std::visit([]<class T>(const auto &) {
                return getElementType<T>();
            }, _value);
        }

        [[nodiscard]] const char *typeName() const {
            return elementTypeName(_value);
        }

        template<StringLike T = std::string>
        [[nodiscard]] auto valueToString() const {
            return toString<T>(_value);
        }

        template <class T = std::string>
        friend auto toString(const ArgumentVariant& v) {
            using ResultType = CharTraitsToString<T>;

            auto quotedLabels = v.labels() | std::views::transform([](const auto& label) {
                return ResultType{"\""} + toCString(label) + "\"";
            });
            auto res = join<T>(quotedLabels, ", ") + ":\n";

            res.append("    Description: " + toString<T>(v._description) + "\n");
            res.append("    Expected types: " + toString<T>(v._expectsMask) + "\n");
            res.append("    Current value: " + toString<T>(v._value) + " (stored as " + v.typeName() + ")");

            return res;
        }
    };

    template<TemplateInstanceOf<std::basic_string> StringType = std::string>
    class ArgumentVariantSet {
        using VariantType = ArgumentVariant<StringType>;
        using MapKey = StringType;
        using MapValue = std::shared_ptr<VariantType>;

        std::map<MapKey, MapValue, std::ranges::less> _variants;

    public:
        ArgumentVariantSet() = default;

        ArgumentVariantSet(std::initializer_list<VariantType> variants) {
            for (const auto &v: variants) {
                add(v);
            }
        }

        void clear() {
            _variants.clear();
        }

        void add(VariantType v) {
            auto p = std::make_shared<VariantType>(std::move(v));
            for (const auto &str: p->labels()) {
                assert(!_variants.contains(str));
                _variants[str] = p;
            }
        }

        auto get(const StringLike auto &label) const -> MapValue {
            auto it = _variants.find(label);
            return it == _variants.end() ? nullptr : it->second;
        }

        auto operator[](const StringLike auto &label) const {
            return get(label);
        }

        template<class T>
        auto getValue(const StringLike auto &label) const {
            auto it = _variants.find(label);
            if (it == _variants.end()) {
                throw std::invalid_argument("Invalid label");
            }
            return it->second->template get<T>();
        }

        template<class T>
        auto getValueOr(const StringLike auto &label, const T &alternative) const try {
            return getValue(label);
        } catch (...) {
            return alternative;
        }

        template<class T>
        auto &operator()(const StringLike auto &label, T &dest) const {
            dest = getValue<T>(label);
            return dest;
        }

        template<class T>
        auto &operator()(const StringLike auto &label, T &dest, const T &alternative) const {
            dest = getValueOr(label, alternative);
            return dest;
        }

        template <class TraitsOrStr> friend auto toString(const ArgumentVariantSet& vs) {
            auto res = CharTraitsToString<TraitsOrStr>{};
            for (auto index = 1; auto&& [k, v]: vs._variants) {
                if (index != 0) {
                    res += '\n';
                }
                res.append("(" + toString<TraitsOrStr>(index++) + ") ");
                res.append(toString<TraitsOrStr>(*v));
            }
            return res;
        }
    };

} //namespace args

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
