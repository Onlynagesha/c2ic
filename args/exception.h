//
// Created by Onlynagesha on 2022/4/5.
//

#ifndef DAWNSEEKER_GRAPH_EXCEPTION_H
#define DAWNSEEKER_GRAPH_EXCEPTION_H

#include <any>
#include <variant>

namespace args {
    class BadVariantAccess : std::bad_variant_access {
        const char *_what = "BadVariantAccess";

    public:
        BadVariantAccess() = default;

        explicit BadVariantAccess(const char *info) : std::bad_variant_access(), _what(info) {}

        [[nodiscard]] const char *what() const noexcept override {
            return _what;
        }
    };

    class BadAnyAccess: std::bad_any_cast {
        const char* _what = "BadAnyAccess";

    public:
        BadAnyAccess() = default;
        explicit BadAnyAccess(const char* what): _what(what) {}

        [[nodiscard]] const char* what() const noexcept override {
            return _what;
        }
    };
}

#endif //DAWNSEEKER_GRAPH_EXCEPTION_H
