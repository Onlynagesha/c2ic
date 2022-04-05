//
// Created by Onlynagesha on 2022/4/5.
//

#ifndef DAWNSEEKER_GRAPH_ARGS_H
#define DAWNSEEKER_GRAPH_ARGS_H

#include <cassert>
#include <map>
#include <set>
#include "variant.h"

namespace args {
    template<utils::TemplateInstanceOf<std::basic_string> StringType = std::string,
            class ContentType_ = BasicVariant<StringType>>
    class BasicArgSet {
    public:
        using ContentType = ContentType_;

    private:
        using MapKey = StringType;
        using MapValue = std::shared_ptr<ContentType>;

        std::set<MapValue, std::owner_less<void>> _variants;
        std::map<MapKey, MapValue, std::ranges::less> _variantMap;

    public:
        BasicArgSet() = default;

        BasicArgSet(std::initializer_list<ContentType> variants) {
            for (const auto &v: variants) {
                add(v);
            }
        }

        void clear() {
            _variants.clear();
            _variantMap.clear();
        }

        void add(ContentType v) {
            auto p = std::make_shared<ContentType>(std::move(v));
            _variants.insert(p);
            for (const auto &str: p->labels()) {
                assert(!_variantMap.contains(str));
                _variantMap[str] = p;
            }
        }

        auto get(const utils::StringLike auto &label) const {
            auto it = _variantMap.find(label);
            return it == _variantMap.end() ? nullptr : it->second;
        }

        auto& operator[](const utils::StringLike auto &label) {
            return *get(label);
        }

        const auto& operator [] (const utils::StringLike auto& label) const {
            return *get(label);
        }

        template<class T>
        auto getValue(const utils::StringLike auto &label) const {
            auto it = _variantMap.find(label);
            if (it == _variantMap.end()) {
                throw std::invalid_argument("Other label");
            }
            return it->second->template get<T>();
        }

        template<class T>
        auto getValueOr(const utils::StringLike auto &label, const T &alternative) const try {
            return getValue(label);
        } catch (...) {
            return alternative;
        }

        template<class T>
        auto &operator()(const utils::StringLike auto &label, T &dest) const {
            dest = getValue<T>(label);
            return dest;
        }

        template<class T>
        auto &operator()(const utils::StringLike auto &label, T &dest, const T &alternative) const {
            dest = getValueOr(label, alternative);
            return dest;
        }

        auto all() {
            return std::ranges::subrange(_variants.begin(), _variants.end());
        }

        auto all() const {
            return std::ranges::subrange(_variants.begin(), _variants.end());
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
}

#endif //DAWNSEEKER_GRAPH_ARGS_H
