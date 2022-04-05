//
// Created by Onlynagesha on 2022/4/5.
//

#ifndef DAWNSEEKER_GRAPH_BASIC_H
#define DAWNSEEKER_GRAPH_BASIC_H

#include <algorithm>
#include <bit>
#include <vector>
#include "utils/string.h"

namespace args {
    struct DescriptionWrapper {
        const char *str;
        const char *endPos;

        constexpr DescriptionWrapper(): str("") {
            endPos = str;
        }
        constexpr DescriptionWrapper(const char* _str, const char* _endPos):
                str(_str), endPos(_endPos) {}

        constexpr DescriptionWrapper(const char* _str, std::size_t n):
                str(_str), endPos(_str + n) {}

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
        AnyArithmetic = 7,
        CaseSensitiveString = 8,
        CaseInsensitiveString = 16,
        AnyString = 24,
        All = 31,
        Other = 1u << 31,
        AllAndOther = (1u << 31) | 31
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

    constexpr inline auto& operator ^= (AlternativeType& L, AlternativeType R) {
        unsigned value = (unsigned)L ^ (unsigned)R;
        L = (AlternativeType)value;
        return L;
    }

    constexpr inline auto operator ^ (AlternativeType L, AlternativeType R) {
        L ^= R;
        return L;
    }

//    constexpr inline int maskSize(AlternativeType A) {
//        return std::popcount((unsigned)(A));
//    }

    constexpr inline int maskSizeNoOther(AlternativeType A) {
        return std::popcount((unsigned)(A & AlternativeType::All));
    }

    constexpr inline bool isNone(AlternativeType A) {
        return A == AlternativeType::None;
    }

    constexpr inline bool isOther(AlternativeType A) {
        return A == AlternativeType::Other;
    }

    template <class T = std::string>
    inline auto toString(AlternativeType A) {
        using ResultType = utils::CharTraitsToString<T>;

        if (A == AlternativeType::None) {
            return ResultType{"None"};
        }

        auto res = ResultType{};
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
        if (!isNone(A & AlternativeType::Other)) {
            append("Other");
        }
        return res;
    }

    namespace literals {
        inline constexpr auto operator ""_expects(const char *typesStr, std::size_t len) {
            auto equalsToEither = [](const char *str, std::size_t n,
                                     std::convertible_to<const char *> auto... args) constexpr {
                return ((n == utils::cstr::strlen(args) &&
                         utils::cstr::ci_strncmp(str, args, n) == std::strong_ordering::equal) || ...);
            };

            auto mask = AlternativeType::None;
            utils::cstr::splitByEither(typesStr, len, ", |", [&](const char *str, std::size_t n) constexpr {
                if (equalsToEither(str, n, "all", "*")) {
                    mask = AlternativeType::All | AlternativeType::Other;
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
                } else if (equalsToEither(str, n, "other", "others")) {
                    mask |= AlternativeType::Other;
                }
            });
            if (mask == AlternativeType::None) {
                throw "Other argument: At least one type shall be supported! " // NOLINT(hicpp-exception-baseclass)
                      "(valid tokens see above)";
            }
            return mask;
        }

        inline constexpr auto operator ""_desc(const char *str, std::size_t len) {
            return DescriptionWrapper{str, str + len};
        }
    }

    template <utils::TemplateInstanceOf<std::basic_string> LabelType_>
    class BasicInfo {
    public:
        using LabelType = LabelType_;
        using LabelListType = std::vector<LabelType>;

    private:
        LabelListType   _labels;
        AlternativeType _mask;
        const char*     _description;

    public:
        explicit BasicInfo(LabelType label,
                           AlternativeType mask = AlternativeType::AllAndOther,
                           const char* desc = ""):
        _labels({std::move(label)}), _mask(mask), _description(desc) {}

        BasicInfo(std::initializer_list<LabelType> labels,
                  AlternativeType mask = AlternativeType::AllAndOther,
                  const char* desc = ""):
        _labels(labels), _mask(mask), _description(desc) {}

        const auto& labels() const {
            return _labels;
        }

        auto mask() const {
            return _mask;
        }

        [[nodiscard]] const char* description() const {
            return _description;
        }

        bool hasLabel(const utils::StringLike auto& str) const {
            return std::ranges::find(_labels, utils::toCString(str)) != _labels.end();
        }
    };
}

#endif //DAWNSEEKER_GRAPH_BASIC_H
