//
// Created by DawnSeeker on 2022/3/25.
//

#include "args.h"
#include "Logger.h"
#include <fstream>

bool parseArgsFromTokens(
        AlgorithmArguments& args, const std::vector<std::string>& tokens, bool isFinal)
{
    auto parser = argparse::ArgumentParser("C2IC Experiment Project");

    parser.add_argument("-graph")
            .help("Path of the graph file")
            .required();

    parser.add_argument("-seedset")
            .help("Path of the seed set file")
            .required();

    parser.add_argument("-k")
            .help("Number of boosted nodes")
            .scan<'i', int>()
            .default_value<int>(10);

    parser.add_argument("-epsilon")
            .help("Epsilon")
            .scan<'g', double>()
            .default_value<double>(0.1);

    parser.add_argument("-lambda")
            .help("Lambda")
            .scan<'g', double>()
            .default_value<double>(0.5);

    parser.add_argument("-ell")
            .help("Ell")
            .scan<'g', double>()
            .default_value<double>(1.0);

    parser.add_argument("-priority")
            .help("Priority of the 4 states. Input from highest to lowest. "
                  "e.g. \"-priority Ca+ Cr- Cr Ca\" refers to Ca+ > Cr- > Cr > Ca")
            .nargs(4)
            .default_value<std::vector<std::string>>({ "Ca+", "Cr-", "Cr", "Ca" });

    try {
        parser.parse_args(tokens);
        args.graphPath = parser.get("-graph");
        args.seedSetPath = parser.get("-seedset");

        args.k = parser.get<int>("-k");
        args.epsilon = parser.get<double>("-epsilon");
        args.lambda = parser.get<double>("-lambda");
        args.ell = parser.get<double>("-ell");

        if (args.epsilon <= 0.0 || args.epsilon >= args.delta) {
            LOG_ERROR("Invalid argument: epsilon must be in (0, delta)!");
            throw std::invalid_argument("epsilon must be in (0, delta)!");
        }
        if (args.lambda < 0.0 || args.lambda > 1.0) {
            LOG_ERROR("Invalid argument: lambda must be in [0, 1]!");
            throw std::invalid_argument("lambda must be in [0, 1]!");
        }
        if (args.ell <= 0.0) {
            LOG_ERROR("Invalid argument: ell must be in (0, +inf)!");
            throw std::invalid_argument("ell must be in (0, +inf)!");
        }

        auto priorityStrs = parser.get<std::vector<std::string>>("-priority");
        if (priorityStrs.size() != 4) {
            throw std::invalid_argument("Exactly 4 arguments for -priority required!");
        }

        static auto matches = {
                std::tuple{"Ca+", &AlgorithmArguments::caPlus,  8},
                std::tuple{"Ca",  &AlgorithmArguments::ca,      4},
                std::tuple{"Cr",  &AlgorithmArguments::cr,      2},
                std::tuple{"Cr-", &AlgorithmArguments::crMinus, 1}
        };

        int readFlag = 0;
        for (int i = 0; i < 4; i++) {
            bool matched = false;
            for (const auto& [token2b, which, mask]: matches) {
                if (priorityStrs[i] == token2b) {
                    matched = true;
                    args.*which = 3 - i;
                    readFlag |= mask;
                }
            }
            if (!matched) {
                throw std::invalid_argument(
                        format("Unexpected priority token '{}': "
                               "must be one of 'Ca+', 'Ca', 'Cr' and 'Cr-'!", priorityStrs[i])
                );
            }
        }
        if (readFlag != 0b1111) {
            throw std::invalid_argument(
                    "Each of 'Ca+', 'Ca', 'Cr' and 'Cr-' must appear exactly once!");
        }

        return true;
    }
    catch (std::exception& e) {
        if (isFinal) {
            LOG_CRITICAL("Failed to parse arguments: "s + e.what());
            std::cerr << parser;
        }
        return false;
    }
}

bool parseArgsFromFile(AlgorithmArguments& args, int argc, char** argv) {
    auto parser = argparse::ArgumentParser();

    parser.add_argument("-config")
            .help("Path of config file")
            .required();

    try {
        parser.parse_args(argc, argv);
        auto path = fs::path(parser.get("-config"));
        auto fin = std::ifstream(path);
        auto tokens = std::vector<std::string>({"(this is a placeholder)"});

        for (auto tk = std::string(); fin >> tk; tokens.push_back(tk)) {}
        return parseArgsFromTokens(args, tokens, false);
    }
    catch (std::exception&) {
        return false;
    }
}

bool parseArgs(AlgorithmArguments& args, int argc, char** argv) {
    // First try to read a config file
    if (parseArgsFromFile(args, argc, argv)) {
        return true;
    }
    // Then try to read argv[]
    auto tokens = std::vector<std::string>();
    for (int i = 0; i < argc; i++) {
        tokens.emplace_back(argv[i]);
    }
    return parseArgsFromTokens(args, tokens, true);
}