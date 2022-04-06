//
// Created by Onlynagesha on 2022/4/6.
//

#ifndef DAWNSEEKER_GRAPH_ARGPARSE_H
#define DAWNSEEKER_GRAPH_ARGPARSE_H

#include <ranges>
#include <string_view>
#include "argparse/argparse.hpp"
#include "args.h"

namespace args {
    namespace helper {
        template <class Range>
        requires(utils::TemplateInstanceOf<std::ranges::range_value_t<Range>, std::basic_string>)
        auto makeArgNameList(Range&& range) {
            auto res = std::vector<std::ranges::range_value_t<Range>>{};

            for (auto&& str: range) {
                res.push_back(str);
                res.push_back("-" + str);
                res.push_back("--" + str);
            }

            return res;
        }
    }

    template <class LabelType, class ContentType>
    inline auto makeParser(const BasicArgSet<LabelType, ContentType>& argSet, const char* programName) {
        auto parser = argparse::ArgumentParser(programName);

        for (const auto& a: argSet.all()) {
            auto argNames = helper::makeArgNameList(a.labels());
            auto argNameViews = argNames | std::views::transform([](const auto& label) {
                return std::string_view{label.data(), label.length()};
            });

            auto& ap = parser.add_argument_from_range(argNameViews).help(utils::toString(a.description()));

            if (a.type() == AlternativeType::None) {
                ap.required();
            } else {
                ap.default_value(a.valueToString());
            }
        }

        return parser;
    }

    template <class LabelType, class ContentType>
    inline void parse(BasicArgSet<LabelType, ContentType>& argSet,
                      argparse::ArgumentParser& parser,
                      const std::vector<std::string>& tokens)
    {
        parser.parse_args(tokens);

        for (auto& a: argSet.all()) {
            a = parser.get(a.labels()[0]);
        }
    }

    template <class LabelType, class ContentType>
    inline void parse(BasicArgSet<LabelType, ContentType>& argSet,
                      argparse::ArgumentParser& parser,
                      int argc, char** argv)
    {
        auto tokens = std::vector<std::string>{};
        for (int i = 0; i < argc; i++) {
            tokens.emplace_back(argv[i]);
        }

        parser.parse_args(tokens);

        for (auto& a: argSet.all()) {
            a = parser.get(utils::string_traits_cast<std::string>(a.labels()[0]));
        }
    }
}

#endif //DAWNSEEKER_GRAPH_ARGPARSE_H
