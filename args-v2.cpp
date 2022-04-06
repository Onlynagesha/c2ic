//
// Created by Onlynagesha on 2022/4/6.
//

#include "args-v2.h"
#include "args/argparse.h"
#include "global.h"
#include "immbasic.h"
#include "utils/numeric.h"

args::CIAnyArgSet makeProgramArgs() {
    using namespace args::literals;

    args::CIAnyArgSet A = {
            {
                    {"graph-path",         "graphPath"},
                    "s"_expects,
                    "Path of the graph file"_desc
            },
            {
                    {"seed-set-path",      "seedSetPath"},
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
                    "cis|?"_expects,
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
                    "u"_expects,
                    "Number of boosted nodes to choose"_desc
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
                    "The maximum number of PRR-sketch samples per center node in SA-IMM algorithm"_desc,
                    utils::halfMax<std::uintmax_t>
            },
            {
                    {"test-times",         "testTimes"},
                    "u"_expects,
                    "How many times to test the solution by forward simulation"_desc,
                    10000
            },
            {
                    {"log-per-percentage", "logPerPercentage"},
                    "f"_expects,
                    "Frequency of debug message during the algorithm (Used for debug logging)"_desc,
                    5.0
            },
            {
                    "delta",
                    "f"_expects,
                    "Algorithm approximation ratio of regular greedy algorithm in PR-IMM and SA-IMM"_desc,
                    1.0 - 1.0 / ns::e
            },
            {
                    {"delta-rg",           "deltaRG"},
                    "f"_expects,
                    "Algorithm approximation ratio of random greedy algorithm in SA-RG-IMM"_desc,
                    1.0 / ns::e
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
    };

    return A;
}

argparse::ArgumentParser makeArgParser(const args::CIAnyArgSet& A) {
    return args::makeParser(A, "C2IC Experiment Project");
}

ProgramArgs prepareProgramArgs(int argc, char** argv) {
    auto argSet = makeProgramArgs();
    auto argParser = makeArgParser(argSet);
    args::parse(argSet, argParser, argc, argv);

    return ProgramArgs{
            .argSet = std::move(argSet),
            .argParser = std::move(argParser)
    };
}

void prepareDerivedArgs(args::CIAnyArgSet& A, std::size_t n) {
    using namespace args::literals;

    A["priority"] = NodePriorityProperty::of(A["priority"].cis());

    A.add({{
                   "n",
                   "u"_expects,
                   "Number of nodes in the graph"_desc,
                   n
           },
           {
                   {"log2n", "log2(n)"},
                   "f"_expects,
                   "(Derived) log2(N) where N is the number of nodes"_desc,
                   std::log2(n)
           },
           {
                   {"lnN",   "ln(N)"},
                   "f"_expects,
                   "(Derived) ln(N) where N is the number of Nodes"_desc,
                   std::log(n)
           },
           {
                   {"lnCnk", "ln(C(n,k))", "ln(Cnk)"},
                   "f"_expects,
                   "(Derived) ln(C(N,k)) where N is the number of nodes, "
                   "k is the number of boosted nodes"_desc,
                   [](std::size_t n, std::size_t k) {
                       double res = 0.0;
                       for (auto x = n - k + 1; x <= n; x++) {
                           res += std::log(x);
                       }
                       for (auto x = 2; x <= k; x++) {
                           res -= std::log(x);
                       }
                       return res;
                   }(n, A.u["k"])
           },
          });

    A["ell"] = A.f["ell"] * (1.0 + ns::ln2 / A.f["lnN"]);

    auto getThetaSA = [&A](auto delta, auto kappa) {
        return (2.0 + 2.0 * kappa / 3.0)
               * (1.0 + delta + kappa)
               * ((A.f["ell"] + 1.0) * A.f["lnN"] + ns::ln2)
               / ((2.0 + delta) * std::pow(kappa, 3.0));
    };

    A.add(
            "alpha",
            "f"_expects,
            "(Derived) Alpha in PR-IMM algorithm"_desc,
            A.f["delta"] * std::sqrt(A.f["ell"] * A.f["lnN"] + ns::ln2)
    ).add(
            "beta",
            "f"_expects,
            "(Derived) Beta in PR-IMM algorithm"_desc,
            std::sqrt(A.f["delta"] * (A.f["ell"] * A.f["lnN"] + A.f["lnCnk"] + ns::ln2))
    ).add(
            {"theta0", "theta0-pr", "theta0PR"},
            "f"_expects,
            "(Derived) Initial delta in PR-IMM algorithm"_desc,
            (1.0 + ns::sqrt2 * A.f["epsilon"] / 3.0)
            * (A.f["lnCnk"] + A.f["ell"] * A.f["lnN"] + std::log(A.f["log2N"]))
            / std::pow(A.f["epsilon"], 2.0)
    ).add(
            {"kappa", "kappa-sa", "kappaSA"},
            "f"_expects,
            "(Derived) kappa in SA-IMM algorithm"_desc,
            A.f["epsilon-sa"] / (2.0 * A.f["delta"] - A.f["epsilon-sa"])
    ).add(
            {"kappa-rg", "kappaRG", "kappa-sa-rg", "kappaSARG"},
            "f"_expects,
            "(Derived) kappa in SA-RG-IMM algorithm"_desc,
            A.f["epsilon-sa"] / (2.0 * A.f["delta-rg"] - A.f["epsilon-sa"])
    ).add(
            {"theta-sa", "thetaSA", "theta0-sa", "theta0SA"},
            "f"_expects,
            "(Derived) theta in SA-IMM algorithm"_desc,
            getThetaSA(A.f["delta"], A.f["kappa"])
    ).add(
            {"theta-sa-rg", "thetaSARG", "theta0-sa-rg", "theta0SARG"},
            "f"_expects,
            "(Derived) theta in SA-RG-IMM algorithm"_desc,
            getThetaSA(A.f["delta-rg"], A.f["kappa-rg"])
    );
}