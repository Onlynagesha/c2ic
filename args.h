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
    // Algorithm approximation ratio = delta - epsilon
    // delta = 1 - 1/e (about 0.632) by default
    // IMM algorithm returns an solution of approximation ratio (delta - epsilon)
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
        constexpr auto width = 10;
        constexpr const char* spec = "{1:>{0}}: {2}\n";

        auto res = std::string("Arguments:\n");
        res += format(spec, width, "graphPath", graphPath);
        res += format(spec, width, "seedSetPath", seedSetPath);

        auto priorityStr = std::string();
        for (int i = 3; i >= 0; i--) {
            if (i != 3) {
                priorityStr += " > ";
            }
            priorityStr += stateOfPriority(i);
        }
        res += format(spec, width, "priority", priorityStr);

        res += format(spec, width, "lambda", lambda);
        res += format(spec, width, "epsilon", epsilon);
        res += format(spec, width, "ell", ell);
        res += format(spec, width, "delta", delta);

        res += format(spec, width, "n", n);
        res += format(spec, width, "k", k);

        res += "Derived arguments:\n";
        res += format(spec, width, "alpha", alpha);
        res += format(spec, width, "beta", beta);
        res += format(spec, width, "theta0", theta0);

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
