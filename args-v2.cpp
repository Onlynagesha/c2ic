//
// Created by Onlynagesha on 2022/4/6.
//

#include "args-v2.h"
#include "args/argparse.h"
#include "immbasic.h"
#include "utils/numeric.h"

ProgramArgs makeProgramArgs() {
    using namespace args::literals;

    ProgramArgs A = {
        {
            {"graph-path",         "graphPath"},
            "s"_expects,
            "Path of the graph file"_desc
        },
        {
            {"seed-set-path",      "seedSetPath",     "seed-path", "seedPath"},
            "s"_expects,
            "Path of the seed set file"_desc
        },
        {
            "algo",
            "cis"_expects,
            "Which algorithm to use"_desc,
            "auto"
        },
        {
            "priority",
            "cis"_expects,
            "Priority sequence of all the node states (Ca+, Ca, Cr and Cr-), "
                "listed from highest to lowest, seperated by spaces, commas or '>'. "
                "e.g. \"Ca+ , Ca > Cr   Cr-\""_desc,
            "Ca+ Cr- Cr Ca"
        },
        {
            "lambda",
            "f"_expects,
            "Lambda of objective function"_desc,
            0.5
        },
        {
            {"k",                  "n-boosted-nodes", "nBoostedNodes"},
            "u|cis"_expects,
            "Number of boosted nodes to choose, provided either as a single unsigned integer, "
            "or a list of unsigned integers separated by spaces, commas or semicolons"_desc
        },
        {
            {"sample-limit",       "sampleLimit"},
            "u"_expects,
            "The maximum number of PRR-sketch samples"_desc,
            utils::halfMax<std::uintmax_t>
        },
        {
            {"sample-limit-sa",    "sampleLimitSA"},
            "u"_expects,
            "The maximum number of samples per center node in SA-IMM algorithm"_desc,
            utils::halfMax<std::uintmax_t>
        },
        {
            {"n-samples",          "nSamples"},
            "u|cis"_expects,
            "Fixed number of PRR-sketch samples in PR-IMM algorithm. "
            "Provided either as a single unsigned integer, "
            "or a list of unsigned integers separated by spaces, commas or semicolons"_desc,
            0 // Default value: do not use fixed sample size
        },
        {
            {"n-samples-sa",       "nSamplesSA"},
            "u|cis"_expects,
            "Fixed number of samples per center node in SA-IMM algorithm. "
            "Provided either as a single unsigned integer, "
            "or a list of unsigned integers separated by spaces, commas or semicolons"_desc,
            0 // Default value: do not use fixed sample size
        },
        {
            {"sample-dist-limit-sa", "sampleDistLimitSA"},
            "u"_expects,
            "The threshold of minimum distance from a sample center node to any of the seeds"_desc,
            utils::halfMax<std::uintmax_t>
        },
        {
            {"test-times",         "testTimes"},
            "u"_expects,
            "How many times to test each boosted node set by forward simulation"_desc,
            10000
        },
        {
            {"greedy-test-times",  "greedyTestTimes"},
            "u"_expects,
            "How many times to test each node in greedy algorithm"_desc,
            1000
        },
        {
            {"log-per-percentage", "logPerPercentage"},
            "f"_expects,
            "Frequency of debug message during the algorithm (Used for debug logging)"_desc,
            5.0
        },
        {
            "ell",
            "f"_expects,
            "Controls the probability of a (delta - epsilonPR)-approximate solution as 1 - n^ell"_desc,
            1.0
        },
        {
            {"epsilon",            "epsilon-pr",      "epsilonPR"},
            "f"_expects,
            "Controls algorithm approximation ratio of PR-IMM algorithm as delta - epsilon"_desc,
            0.1
        },
        {
            {"epsilon-sa",         "epsilonSA"},
            "f"_expects,
            "Controls algorithm approximation ratio of SA-IMM algorithm as delta - epsilonSA, "
                "or SA-RG-IMM algorithm as deltaRG - epsilonSA"_desc,
            0.1
        },
        {
            {"gain-threshold-sa",  "gainThresholdSA"},
            "f"_expects,
            "Minimum average gain that a node s should contribute to certain center node v "
                "in SA-IMM or SA-RG-IMM algorithm"_desc,
            0.0
        },
        {
            {"j",                  "n-threads",       "nThreads"},
            "u"_expects,
            "Number of threads used in multi-threading task"_desc,
            1
        }
    };

    return A;
}

argparse::ArgumentParser makeArgParser(const ProgramArgs& A) {
    return args::makeParser(A, "C2IC Experiment Project");
}

ProgramArgs prepareProgramArgs(int argc, char** argv) {
    auto argSet    = makeProgramArgs();
    auto argParser = makeArgParser(argSet);
    args::parse(argSet, argParser, argc, argv);

    return argSet;
}

AlgorithmArgsPtr getAlgorithmArgs(std::size_t n, const ProgramArgs& args) {
    auto algo = getAlgorithmLabel(args);

    switch (algo) {
    case AlgorithmLabel::PR_IMM:
        if (sampleSizeIsFixed(args, AlgorithmLabel::PR_IMM)) {
            return std::make_unique<StaticArgs_PR_IMM>(n, args);
        } else {
            return std::make_unique<DynamicArgs_PR_IMM>(n, args);
        }
    case AlgorithmLabel::SA_IMM:
    case AlgorithmLabel::SA_RG_IMM:
        return std::make_unique<Args_SA_IMM>(n, args);
    case AlgorithmLabel::Greedy:
        return std::make_unique<GreedyArgs>(n, args);
    default:
        return std::make_unique<BasicArgs>(n, args);
    }
}