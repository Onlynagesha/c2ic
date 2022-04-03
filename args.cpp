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

    parser.add_argument("-algo")
            .help("The algorithm to use: 'Auto', 'PR-IMM', 'SA-IMM' or 'SA-RG-IMM'")
            .default_value(std::string("Auto"));

    parser.add_argument("-k")
            .help("Number of boosted nodes")
            .scan<'i', int>()
            .default_value<int>(10);

    parser.add_argument("-sample-limit")
            .help("Maximum number of PRR-sketch samples")
            .scan<'u', std::size_t>()
            .default_value<std::size_t>(halfMax<std::size_t> + 0u);

    parser.add_argument("-theta-sa-limit")
            .help("Limit of theta_sa in SA-IMM algorithm")
            .scan<'u', std::size_t>()
            .default_value<std::size_t>(halfMax<std::size_t> + 0u);

    parser.add_argument("-test-times")
            .help("How many times to check the solution by forward simulation")
            .scan<'u', std::size_t>()
            .default_value<std::size_t>(10000);

    parser.add_argument("-log-per-percentage")
            .help("Frequency for debug logging")
            .scan<'g', double>()
            .default_value<double>(5.0);

    parser.add_argument("-epsilon", "-epsilon1", "-epsilon-pr")
            .help("Epsilon1 in PR-IMM algorithm")
            .scan<'g', double>()
            .default_value<double>(0.1);

    parser.add_argument("-epsilon2", "-epsilon-sa")
            .help("Epsilon2 in SA-IMM algorithm")
            .scan<'g', double>()
            .default_value<double>(halfMax<double> + 0.0);

    parser.add_argument("-gain-threshold-sa")
            .help("Minimum average gain for each node in SA-IMM algorithm")
            .scan<'g', double>()
            .default_value<double>(0.0);

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
        args.algo = ci_string(parser.get("-algo").c_str()); // NOLINT(readability-redundant-string-cstr)

        static const char* delims = " _,;";
        // Replaces "PR_IMM" to "PR-IMM" for example
        for (std::size_t pos = args.algo.find_first_of(delims);
                pos != std::string::npos && pos != args.algo.length();
                pos = args.algo.find_first_of(delims, pos + 1)) {
            args.algo[pos] = '-';
        }

        static auto validAlgoNames = {"auto", "pr-imm", "sa-imm", "sa-rg-imm"};
        if (rs::count(validAlgoNames, args.algo) == 0) {
            LOG_ERROR("Invalid algorithm name!");
            throw std::invalid_argument("Invalid algorithm name!");
        }

        args.k = parser.get<int>("-k");
        args.sampleLimit = parser.get<std::size_t>("-sample-limit");
        args.thetaSALimit = parser.get<std::size_t>("-theta-sa-limit");
        args.testTimes = parser.get<std::size_t>("-test-times");
        args.logPerPercentage = parser.get<double>("-log-per-percentage");
        args.epsilon_pr = parser.get<double>("-epsilon-pr");
        args.epsilon_sa = parser.get<double>("-epsilon-sa");
        // epsilon2 = epsilon1 by default
        if (args.epsilon_sa == halfMax<double>) {
            args.epsilon_sa = args.epsilon_pr;
        }
        args.lambda = parser.get<double>("-lambda");
        args.ell = parser.get<double>("-ell");
        args.gainThreshold_sa = parser.get<double>("-gain-threshold-sa");

        if (args.logPerPercentage <= 0.0 || args.logPerPercentage > 100.0) {
            LOG_WARNING("Invalid argument: logPerPercentage should be in (0, 100]. Related debug log is disabled.");
            args.logPerPercentage = 200.0;
        }

        if (args.epsilon_pr <= 0.0 || args.epsilon_pr >= args.delta) {
            LOG_ERROR("Invalid argument: epsilon_pr must be in (0, delta)!");
            throw std::invalid_argument("epsilon_pr must be in (0, delta)!");
        }
        if (args.epsilon_sa <= 0.0 || args.epsilon_sa >= args.delta) {
            LOG_ERROR("Invalid argument: epsilon_sa must be in (0, delta)!");
            throw std::invalid_argument("epsilon_sa must be in (0, delta)!");
        }
        if (args.gainThreshold_sa < 0.0 || args.gainThreshold_sa >= 1.0) {
            LOG_ERROR("Invalid argument: gainThreshold_sa must be in [0, 1)!");
            throw std::invalid_argument("gainThreshold_sa must be in [0, 1)!");
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