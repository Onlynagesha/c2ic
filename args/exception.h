//
// Created by Onlynagesha on 2022/4/5.
//

#ifndef DAWNSEEKER_GRAPH_EXCEPTION_H
#define DAWNSEEKER_GRAPH_EXCEPTION_H

#include <any>
#include <variant>

namespace args {
    class BadArgumentVariantAccess : std::bad_variant_access {
        const char *_what = "BadArgumentVariantAccess";

    public:
        BadArgumentVariantAccess() = default;

        explicit BadArgumentVariantAccess(const char *info) : std::bad_variant_access(), _what(info) {}

        [[nodiscard]] const char *what() const noexcept override {
            return _what;
        }
    };

    class BadArgumentAnyAccess: std::bad_any_cast {
        const char* _what = "BadArgumentAnyAccess";

    public:
        BadArgumentAnyAccess() = default;
        explicit BadArgumentAnyAccess(const char* what): _what(what) {}

        [[nodiscard]] const char* what() const noexcept override {
            return _what;
        }
    };
}

#endif //DAWNSEEKER_GRAPH_EXCEPTION_H
