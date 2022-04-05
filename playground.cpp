//
// Created by Onlynagesha on 2022/4/3.
//

#include "args/args.h"
#include "utils.h"
#include <any>
#include <iomanip>

namespace args {
}

int _main() {
    using namespace args::literals;

    std::cout << std::boolalpha;

//    struct ProgramArguments {
//        // Path of the graph file
//        std::string             graphPath;
//        // Path of the seed set file
//        std::string             seedSetPath;
//        // Which algorithm to use
//        // "auto": decided by node state priority
//        // "pr-imm": PR-IMM algorithm (for M-S)
//        // "sa-imm": SA-IMM algorithm (for M-nS)
//        // "sa-rg-imm": SA-RG-IMM algorithm (for nM-nS)
//        ci_string   algo;
//        // Priority values of all the node states (Ca+, Ca, Cr, Cr-, and None)
//        NodeStatePriorityArray  priority;
//        // Controls the weights that
//        //  positive message and negative message contribute respectively
//        // See nodeStateGain[] and setNodeStateGain(lambda) for details
//        double          lambda;
//        // Number of boosted nodes to choose
//        std::size_t     k;
//        // The maximum number of PRR-sketch samples
//        std::size_t     sampleLimit;
//        // The maximum number of PRR-sketches per center node v in SA-IMM algorithm
//        std::size_t     sampleLimitSA;
//    };

    auto A = args::CIAnyArgSet{{
            {
                    {"graph-path", "graphPath"},
                    "s"_expects,
                    "Path of the graph file"_desc
            },
            {
                    {"seed-set-path", "seedSetPath"},
                    "s"_expects,
                    "Path of the seed set file"_desc
            },
            {
                    "algo",
                    "cis"_expects,
                    "Which algorithm to use"_desc
            },
            {
                    "priority",
                    "cis"_expects,
                    "Priority sequence of all the node states (Ca+, Ca, Cr and Cr-), "
                    "listed from highest to lowest, seperated by spaces, commas or '>'. "
                    "e.g. \"Ca+ , Ca > Cr   Cr-\""_desc
            },
            {
                    "lambda",
                    "f"_expects,
                    "Lambda of objective function"_desc
            },
            {
                    {"k", "n-boosted-nodes", "nBoostedNodes"},
                    "i|u"_expects,
                    "Number of boosted nodes to choose"_desc
            },
    }};

    A["k"] = 10;
    A["Lambda"] = "0.123456";
    A["priority"] = "Ca+ Ca Cr- Cr";

    A.add({"foobar", "*"_expects, "Foo bar"_desc, std::vector<int>{{1, 2, 3, 4, 5}}});

    if (A["Priority"].compare("ca+ ca cr- cr") != 0) {
        throw std::logic_error("Error in case-insensitive comparison");
    }
    std::cout << toString(A) << std::endl;

    return 0;
}

int main() {
    try {
        return _main();
    } catch (std::exception& e) {
        std::cerr << e.what() << std::endl;
        return 0;
    }
}