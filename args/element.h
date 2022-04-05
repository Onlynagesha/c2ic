//
// Created by Onlynagesha on 2022/4/5.
//

#ifndef DAWNSEEKER_GRAPH_ELEMENT_H
#define DAWNSEEKER_GRAPH_ELEMENT_H

#include <variant>
#include "basic.h"
#include "exception.h"
#include "utils/assert.h"
#include "utils/numeric.h"
#include "utils/string.h"
#include "utils/typeinfo.h"

namespace args {
    using VariantElement = std::variant<
            std::monostate,         // No value yet
            std::intmax_t,          // Signed integer
            std::uintmax_t,         // Unsigned integer
            long double,            // Floating point
            std::string,            // String
            utils::ci_string        // String, but case-insensitive
    >;

    template<class T_>
    inline constexpr auto getElementType() {
        using T = std::remove_cvref_t<T_>;

        if constexpr (std::is_same_v<T, std::monostate>) {
            return AlternativeType::None;
        } else if constexpr (std::is_integral_v<T>) {
            if constexpr (std::is_signed_v<T>) {
                return AlternativeType::SignedInteger;
            } else {
                return AlternativeType::UnsignedInteger;
            }
        } else if constexpr (std::is_floating_point_v<T>) {
            return AlternativeType::FloatingPoint;
        } else if constexpr (std::is_same_v<T, utils::ci_string>) {
            return AlternativeType::CaseInsensitiveString;
        } else if constexpr (utils::StringLike<T>) {
            return AlternativeType::CaseSensitiveString;
        } else {
            return AlternativeType::Other;
        }
    }

    inline auto getElementType(const VariantElement &v) {
        return std::visit([]<class T>(const T &) {
            return getElementType<T>();
        }, v);
    }

    template<class T>
    inline auto toElementValue(const T &value) {
        constexpr auto tv = getElementType<T>();

        if constexpr (tv == AlternativeType::None) {
            return std::monostate{};
        } else if constexpr (tv == AlternativeType::SignedInteger) {
            return static_cast<std::intmax_t>(value);
        } else if constexpr (tv == AlternativeType::UnsignedInteger) {
            return static_cast<std::uintmax_t>(value);
        } else if constexpr (tv == AlternativeType::FloatingPoint) {
            return static_cast<long double>(value);
        } else if constexpr (tv == AlternativeType::CaseInsensitiveString) {
            return utils::ci_string{utils::toCString(value)};
        } else if constexpr (tv == AlternativeType::CaseSensitiveString) {
            return std::string{utils::toCString(value)};
        } else {
            static_assert(utils::AlwaysFalse<T>::value,
                          "Only integers, floating-points, and strings "
                          "(either case sensitive or insensitive) are supported!");
        }
    }

    namespace helper {
        // Requires: !isNone(mask & AnyString)
        // Case-insensitive first
        template <class T>
        inline auto toStringElement(AlternativeType mask, const T& value) -> VariantElement {
            if (!isNone(mask & AlternativeType::CaseInsensitiveString)) {
                return utils::toString<utils::ci_string>(value);
            } else {
                return utils::toString(value);
            }
        }
    }

    template <std::integral IntType>
    inline auto toElementFromInteger(AlternativeType mask, IntType value) -> VariantElement {
        constexpr auto tv = getElementType<IntType>();
        // (1) Exactly matches
        if (!isNone(tv & mask)) {
            return toElementValue(value);
        }
        // (2) Matches to any string type
        if (!isNone(mask & AlternativeType::AnyString)) {
            return helper::toStringElement(mask, value);
        }
        // (3) Try the other integer type
        constexpr auto otherIntTv = tv ^ AlternativeType::AnyInteger;
        using OtherInt = std::conditional_t<
                otherIntTv == AlternativeType::SignedInteger, std::intmax_t, std::uintmax_t>;
        if (!isNone(mask & otherIntTv)) try {
                return utils::value_safe_arithmetic_cast<OtherInt>(value);
            } catch (...) {}

        // (4) Try floating point type
        // This time do not try and catch, if an exception is raised, just throw it
        if (!isNone(mask & AlternativeType::FloatingPoint)) {
            return utils::value_safe_arithmetic_cast<long double>(value);
        }

        constexpr const char* what = "Failed to convert to an VariantElement from integer";
        throw std::invalid_argument(what);
    }

    template <std::floating_point FloatType>
    inline auto toElementFromFloating(AlternativeType mask, FloatType value) -> VariantElement {
        constexpr auto tv = getElementType<FloatType>();

        // (1) Exactly matches
        if (! isNone(tv & mask)) {
            return std::monostate{};
        }
        // (2) Matches to any string type
        if (!isNone(mask & AlternativeType::AnyString)) {
            return helper::toStringElement(value);
        }
        // (3) Try signed integer
        if (!isNone(mask & AlternativeType::SignedInteger)) try {
                return utils::value_safe_arithmetic_cast<std::intmax_t>(value);
            } catch (...) {}

        // (4) Try unsigned integer
        // This time does not catch exception, just let it throw
        if (!isNone(mask & AlternativeType::UnsignedInteger)) {
            return utils::value_safe_arithmetic_cast<std::uintmax_t>(value);
        }

        constexpr auto what = "Failed to convert to an VariantElement from floating point";
        throw std::invalid_argument(what);
    }

    template <utils::StringLike StringType>
    inline auto toElementFromString(AlternativeType mask, const StringType& value) -> VariantElement {
        constexpr auto tv = getElementType<StringType>();

        // (1) Exactly matches
        if (! isNone(tv & mask)) {
            return std::monostate{};
        }
        // (2) String of the another type
        constexpr auto otherStrTv = tv ^ AlternativeType::AnyString;
        using OtherString = std::conditional_t<
                otherStrTv == AlternativeType::CaseInsensitiveString, utils::ci_string, std::string>;
        if (! isNone(mask & otherStrTv)) {
            return OtherString{utils::toCString(value)};
        }
        // (3) Try floating point
        if (!isNone(mask & AlternativeType::FloatingPoint)) try {
                return utils::fromStringStrict<long double>(value);
            } catch (...) {}
        // (4) Try signed integer
        if (!isNone(mask & AlternativeType::SignedInteger)) try {
                return utils::fromStringStrict<std::intmax_t>(value);
            } catch (...) {}
        // (5) Try unsigned integer
        // This time does not catch the exception, just throw it
        if (!isNone(mask & AlternativeType::UnsignedInteger)) {
            return utils::fromStringStrict<std::uintmax_t>(value);
        }

        constexpr auto what = "Failed to convert to an VariantElement from floating point";
        throw std::invalid_argument(what);
    }

    template <class T>
    inline auto toElement(AlternativeType mask, const T& value) -> VariantElement {
        constexpr auto tv = getElementType<T>();

        if constexpr (tv == AlternativeType::None) {
            return std::monostate{};
        } else if constexpr (!isNone(tv & AlternativeType::AnyInteger)) {
            return toElementFromInteger(mask, value);
        } else if constexpr (!isNone(tv & AlternativeType::FloatingPoint)) {
            return toElementFromFloating(mask, value);
        } else if constexpr (!isNone(tv & AlternativeType::AnyString)) {
            return toElementFromString(mask, value);
        } else {
            throw std::invalid_argument("Failed to convert to an VariantElement from an unrecognized type");
        }
    }

    inline auto toElementValue() {
        return std::monostate{};
    }

    template<class U, utils::SameAsOneOfWithoutCVRef<VariantElement> T>
    inline U fromElementValue(const T &value) {
        // std::monostate: always throws an exception
        if constexpr (std::is_same_v<T, std::monostate>) {
            throw BadVariantAccess("Visits std::monostate (i.e. no value yet)");
        }
            // Converts from an arithmetic value
        else if constexpr (std::is_arithmetic_v<T>) {
            if constexpr (utils::TemplateInstanceOf<U, std::basic_string>) {
                // String <- Arithmetic, where String must be an instance of std::basic_string
                // (in other words, const char* is not supported)
                return utils::toString<U>(value);
            } else if constexpr (std::is_arithmetic_v<U>) {
                // Arithmetic <- Arithmetic
                // Exception may be thrown if underflow or overflow happens
                return utils::value_safe_arithmetic_cast<U>(value);
            } else {
                // Destination type U is invalid (e.g. const char*)
                static_assert(utils::AlwaysFalse<U>::value,
                              "U must be either arithmetic, or an instance of std::basic_string");
            }
        }
            // Converts from a string
        else if constexpr (utils::StringLike<T>) {
            if constexpr (utils::TemplateInstanceOf<U, std::basic_string>) {
                // String <- String
                return utils::string_traits_cast<U>(value);
            } else if constexpr (std::is_arithmetic_v<U>) {
                // Arithmetic <- String (Exception may be throw if fails)
                return utils::fromStringStrict<U>(value);
            } else {
                // Destination type U is invalid (e.g. const char*)
                static_assert(utils::AlwaysFalse<U>::value,
                              "U must be either arithmetic, or an instance of std::basic_string");
            }
        }
            // Unsupported T
        else {
            static_assert(utils::AlwaysFalse<T>::value, "Unimplemented or invalid type T as input");
        }
    }

    template<class U, utils::SameAsOneOf<VariantElement> T>
    inline auto fromElementValueOr(const T &value, const U &alternative) noexcept try {
        return fromElementValue<U>(value);
    } catch (...) {
        return alternative;
    }

    template<class T, class U>
    inline auto compareElementValues(const T &A, const U &B) noexcept -> std::partial_ordering {
        constexpr auto vT = getElementType<T>();
        constexpr auto vU = getElementType<U>();

        // First check std::monostate (i.e. no value)
        // No-value is regarded the lowest
        constexpr auto mT = (int) (vT == AlternativeType::None);
        constexpr auto mU = (int) (vU == AlternativeType::None);
        if constexpr (mT || mU) {
            return (1 - mT) <=> (1 - mU);
        }
            // Then check string, two strings are compared as case-insensitive of either is ci_string
            // Otherwise, compare case-sensitively
        else if constexpr (!isNone(vT & AlternativeType::AnyString) &&
                           !isNone(vU & AlternativeType::AnyString)) {
            if constexpr (!isNone((vT | vU) & AlternativeType::CaseInsensitiveString)) {
                return utils::cstr::ci_strcmp(std::ranges::cdata(A), std::ranges::cdata(B));
            } else {
                return utils::cstr::strcmp(utils::toCString(A), utils::toCString(B));
            }
        }
            // Then for arithmetic types, two numbers are compared by their values
            //  with (possibly) implicit conversion
        else if constexpr (!isNone(vT & AlternativeType::AnyArithmetic) &&
                           !isNone(vU & AlternativeType::AnyArithmetic)) {
            if constexpr (!isNone(vT & AlternativeType::AnyInteger) && !isNone(vU & AlternativeType::AnyInteger)) {
                // If both are integers, compares with the value-safe functions
                return std::cmp_equal(A, B) ? std::strong_ordering::equal :
                       std::cmp_less(A, B) ? std::strong_ordering::less : std::strong_ordering::greater;
            } else {
                // Otherwise, just let they get converted implicitly
                return A <=> B;
            }
        }
            // Then for arithmetic-string mixed comparison
            // Transforms the string to the arithmetic type. If conversion fails, returns unordered
        else if constexpr (!isNone((vT | vU) & AlternativeType::AnyString)) try {
                if constexpr (!isNone(vT & AlternativeType::AnyString)) {
                    return utils::fromStringStrict<U>(A) <=> B;    // (String, Arithmetic)
                } else {
                    return A <=> utils::fromStringStrict<T>(B);    // (Arithmetic, String)
                }
            } catch (...) {
                return std::partial_ordering::unordered;    // Conversion fails
            }
            // Unsupported cases
        else {
            static_assert(utils::AlwaysFalse<T>::value, "Unsupported or invalid types");
        }
    }

    inline std::partial_ordering compareElements(
            const VariantElement &A, const VariantElement &B) noexcept {
        const auto cmp = [](const auto &a, const auto &b) {
            return compareElementValues(a, b);
        };
        return std::visit(cmp, A, B);
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
        using ResultType = utils::CharTraitsToString<TraitsOrStr>;

        return std::visit([]<class T>(const T &x) -> ResultType {
            constexpr auto tv = getElementType<T>();

            if constexpr (tv == AlternativeType::None) {
                return "(No value yet)";
            } else if constexpr (!isNone(tv & AlternativeType::AnyString)) {
                return "\"" + fromElementValue<ResultType>(x) + "\"";
            } else {
                return fromElementValue<ResultType>(x);
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
                return utils::integralTypeName<T>();
            } else if constexpr (std::same_as<T, long double>) {
                return "long double";
            } else if constexpr (std::same_as<T, std::string>) {
                return "std::string";
            } else if constexpr (std::same_as<T, utils::ci_string>) {
                return "std::basic_string<char, utils::ci_char_traits>";
            } else {
                static_assert(utils::AlwaysFalse<T>::value, "Unimplemented or invalid element type");
            }
        }
    }

    inline constexpr const char *elementTypeName(const VariantElement &v) {
        return std::visit([]<class T>(const T &) { return elementTypeName<T>(); }, v);
    }
}

#endif //DAWNSEEKER_GRAPH_ELEMENT_H
