//
// Created by Onlynagesha on 2022/4/5.
//

#ifndef DAWNSEEKER_GRAPH_ARGS_H
#define DAWNSEEKER_GRAPH_ARGS_H

#include <cassert>
#include <map>
#include <set>
#include "any.h"
#include "variant.h"

namespace args {
    template <utils::TemplateInstanceOf<std::basic_string> LabelType = std::string,
            class ContentType_ = BasicVariant<LabelType>>
    class BasicArgSet {
    public:
        using ContentType = ContentType_;

    private:
        using MapKey = LabelType;
        using MapValue = std::shared_ptr<ContentType>;

        std::set<MapValue, std::owner_less<void>> _variants;
        std::map<MapKey, MapValue, std::ranges::less> _variantMap;

        struct VisitorCtorTag {};
        static constexpr VisitorCtorTag visitorCtorTag{};

        template <class ValueType>
        struct ContentVisitor {
            using ParentType = BasicArgSet;
            const ParentType* parent = nullptr;

            explicit ContentVisitor(const ParentType* p): parent(p) {}

            ValueType operator [] (const utils::StringLike auto& label) const {
                return parent->getValue<ValueType>(label);
            }

            ValueType operator () (const utils::StringLike auto& label, const ValueType& alternative) const {
                return parent->getValueOr(label, alternative);
            }
        };

    public:
        ContentVisitor<std::intmax_t> i;
        ContentVisitor<std::uintmax_t> u;
        ContentVisitor<long double> f;
        ContentVisitor<std::string> s;
        ContentVisitor<utils::ci_string> cis;

    private:
        explicit BasicArgSet(VisitorCtorTag): i(this), u(this), f(this), s(this), cis(this) {}

    public:
        BasicArgSet(): BasicArgSet(visitorCtorTag) {}

        BasicArgSet(std::initializer_list<ContentType> variants): BasicArgSet(visitorCtorTag) {
            for (const auto& v: variants) {
                add(v);
            }
        }

        BasicArgSet(const BasicArgSet& other): BasicArgSet(visitorCtorTag) {
            _variants = other._variants;
            _variantMap = other._variantMap;
        }

        BasicArgSet(BasicArgSet&& other) noexcept : BasicArgSet(visitorCtorTag) {
            _variants = std::move(other._variants);
            _variantMap = std::move(other._variantMap);
        }

        void clear() {
            _variants.clear();
            _variantMap.clear();
        }

        auto& add(ContentType v) {
            auto p = std::make_shared<ContentType>(std::move(v));
            _variants.insert(p);
            for (const auto& str: p->labels()) {
                assert(!_variantMap.contains(str));
                _variantMap[str] = p;
            }
            return *this;
        }

        template <class... Args>
        requires std::is_constructible_v<ContentType, LabelType, Args...>
        auto& add(LabelType label, Args&& ... args) {
            add(ContentType(std::move(label), std::forward<Args>(args)...));
            return *this;
        }

        template <class... Args>
        requires std::is_constructible_v<ContentType, std::initializer_list<LabelType>, Args...>
        auto& add(std::initializer_list<LabelType> labels, Args&& ... args) {
            add(ContentType(labels, std::forward<Args>(args)...));
            return *this;
        }

        auto& add(std::initializer_list<ContentType> vs) {
            for (auto v: vs) {
                add(std::move(v));
            }
            return *this;
        }

        auto get(const utils::StringLike auto& label) const {
            auto it = _variantMap.find(label);
            return it == _variantMap.end() ? nullptr : it->second;
        }

        auto& operator [](const utils::StringLike auto& label) {
            using namespace std::string_literals;

            auto p = get(label);
            if (p == nullptr) {
                throw std::invalid_argument("Unrecognized label: "s + utils::toString(label));
            }
            return *get(label);
        }

        const auto& operator [](const utils::StringLike auto& label) const {
            return const_cast<BasicArgSet*>(this)->template operator [](label);
        }

        template <class T>
        auto getValue(const utils::StringLike auto& label) const {
            using namespace std::string_literals;

            auto it = _variantMap.find(label);
            if (it == _variantMap.end()) {
                throw std::invalid_argument("Unrecognized label: "s + utils::toString(label));
            }
            return it->second->template get<T>();
        }

        template <class T>
        auto getValueOr(const utils::StringLike auto& label, const T& alternative) const try {
            return getValue(label);
        } catch (...) {
            return alternative;
        }

        template <class T>
        auto& operator ()(const utils::StringLike auto& label, T& dest) const {
            dest = getValue<T>(label);
            return dest;
        }

        template <class T>
        auto& operator ()(const utils::StringLike auto& label, T& dest, const T& alternative) const {
            dest = getValueOr(label, alternative);
            return dest;
        }

        auto all() {
            return _variants | std::views::transform([](const auto& p) -> auto& { return *p; });
        }

        auto all() const {
            return _variants | std::views::transform([](const auto& p) -> const auto& { return *p; });
        }

        template <class TraitsOrStr = std::string>
        friend auto toString(const BasicArgSet& vs) {
            auto res = utils::CharTraitsToString<TraitsOrStr>{};
            for (auto index = 1; const auto& v: vs._variants) {
                if (index != 0) {
                    res += '\n';
                }
                res.append("(" + utils::toString<TraitsOrStr>(index++) + ") ");
                res.append(toString<TraitsOrStr>(*v));
            }
            return res;
        }
    };

    using ArgSet = BasicArgSet<std::string, BasicVariant<std::string>>;
    using CIArgSet = BasicArgSet<utils::ci_string, BasicVariant<utils::ci_string>>;

    using AnyArgSet = BasicArgSet<std::string, BasicAny<std::string>>;
    using CIAnyArgSet = BasicArgSet<utils::ci_string, BasicAny<utils::ci_string>>;
}

#endif //DAWNSEEKER_GRAPH_ARGS_H
