//
// Created by DawnSeeker on 2022/3/25.
//

#ifndef GRAPH_ARGS_H
#define GRAPH_ARGS_H

#include "argparse/argparse.hpp"
#include "global.h"
#include "immbasic.h"
#include <optional>

struct AlgorithmArguments {
    // Path of the graph file
    std::string     graphPath;
    // Path of the seed set file
    std::string     seedSetPath;
    // Priority values of the 4 node states
    // Default: Ca+ > Cr- > Cr > Ca
    // (whose gain is the upper bound for all 24 combinations)
    // See setNodeState(caPlus, ca, cr, crMinus) for details
    int             caPlus = 3;
    int             ca = 0;
    int             cr = 1;
    int             crMinus = 2;
    // Controls the weights that
    //  positive message and negative message contribute respectively
    // See nodeStateGain[] and setNodeStateGain(lambda) for details
    double          lambda;
    // Number of nodes
    std::size_t     n;
    // Number of boosted nodes to choose
    std::size_t     k;
    // The maximum number of PRR-sketch samples
    std::size_t     sampleLimit = halfMax<std::size_t>;
    // How many times to test the solution by forward simulation
    std::size_t     testTimes = 10000;
    // Algorithm approximation ratio = delta - epsilon
    // delta = 1 - 1/e (about 0.632) by default
    // IMM algorithm returns a solution of approximation ratio (delta - epsilon)
    //  WITH at least 1 - (1/n)^ell probability
    double          delta = 1.0 - 1.0 / ns::e;
    // Controls the probability of an (delta - epsilon)-approximate solution
    // The higher ell, the lower probability for a low-quality result,
    //  but the higher time & space consumption
    // ell = 1.0 by default
    double          ell = 1.0;
    // Controls the approximation ratio
    // The lower delta, the higher approximation ratio (i.e. higher quality result)
    //  but the higher time & space consumption
    double          epsilon;

    // The following parameters are derived from those above
    // See .updateValues() for details
    double          alpha;
    double          beta;
    double          theta0;

    double          log2N;      // log2(n)
    double          lnN;        // ln(n)
    double          lnCnk;      // ln(C(n, k))

    void updateValues() {
        log2N = std::log2(n);
        lnN = std::log(n);

        lnCnk = 0;
        for (auto x = n - k + 1; x <= n; x++) {
            lnCnk += std::log(x);
        }
        for (auto x = 1; x <= k; x++) {
            lnCnk -= std::log(x);
        }

        alpha = delta * std::sqrt(ell * lnN + ns::ln2);
        beta = std::sqrt(delta * (ell * lnN + lnCnk + ns::ln2));
        theta0 = (1.0 + ns::sqrt2 * epsilon / 3.0)
                 * (lnCnk + ell * lnN + std::log(log2N))
                 / std::pow(epsilon, 2.0);
    }

    void updateValues(std::size_t _n) {
        this->n = _n;
        updateValues();
    }

    void updateGainAndPriority() const {
        setNodeStateGain(lambda);
        setNodeStatePriority(caPlus, ca, cr, crMinus);
    }

    [[nodiscard]] const char* stateOfPriority(int priority) const {
        if (caPlus == priority) {
            return "Ca+";
        }
        if (ca == priority) {
            return "Ca";
        }
        if (cr == priority) {
            return "Cr";
        }
        if (crMinus == priority) {
            return "Cr-";
        }
        return "ERROR";
    }

    [[nodiscard]] std::string dump() const {
        auto doFormat = [this](std::initializer_list<std::pair<std::string, std::string>> list) {
            auto maxLen = rs::max(list | vs::keys | vs::transform(&std::string::length));
            auto fmt = format("{{0:>{0}}}: {{1}}\n", maxLen);
            auto res = std::string{};
            for (const auto& [name, value]: list) {
                res += format(fmt, name, value);
            }
            return res;
        };

        auto priorityStr = std::string();
        for (int i = 3; i >= 0; i--) {
            if (i != 3) {
                priorityStr += " > ";
            }
            priorityStr += stateOfPriority(i);
        }

        auto res = std::string("Arguments:\n") + doFormat({
            {"graphPath", graphPath},
            {"seedSetPath", seedSetPath},
            {"priority", priorityStr},
            {"lambda", toString(lambda)},
            {"epsilon", toString(epsilon)},
            {"ell", toString(ell)},
            {"delta", toString(delta)},
            {"n", toString(n)},
            {"k", toString(k)},
            {"sampleLimit", toString(sampleLimit)},
            {"testTimes", toString(testTimes)}
        }) + "Derived arguments:\n" + doFormat({
            {"alpha", toString(alpha)},
            {"beta", toString(beta)},
            {"theta0", toString(theta0)}
        });
        // Removes one trailing '\n'
        res.pop_back();
        return res;
    }
};

// Gets the arguments from the command line
// Returns: successful or not
bool parseArgs(AlgorithmArguments& args, int argc, char** argv);

// Gets the arguments from the command line
// Returns: args if successful, std::nullopt of not
inline std::optional<AlgorithmArguments> parseArgs(int argc, char** argv) {
    auto args = AlgorithmArguments{};
    return parseArgs(args, argc, argv) ? std::make_optional(args) : std::nullopt;
}

#endif //GRAPH_ARGS_H
