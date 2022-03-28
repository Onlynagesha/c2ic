//
// Created by DawnSeeker on 2022/3/25.
//

#ifndef GRAPH_ARGS_H
#define GRAPH_ARGS_H

#include "argparse/argparse.hpp"
#include "global.h"
#include "immbasic.h"
#include "utils.h"
#include <optional>

struct AlgorithmArguments {
    // Path of the graph file
    std::string             graphPath;
    // Path of the seed set file
    std::string             seedSetPath;
    // Which algorithm to use
    // "auto": decided by node state priority
    // "pr-imm": PR-IMM algorithm (for M-S)
    // "sa-imm": SA-IMM algorithm (for M-nS)
    // "sa-rg-imm": SA-RG-IMM algorithm (for nM-nS)
    CaseInsensitiveString   algo;
    // Priority values of the 4 node states
    // Default: Ca+ > Cr- > Cr > Ca
    // (whose gain is the upper bound for all 24 combinations)
    // See setNodeState(caPlus, ca, cr, crMinus) for details
    int             caPlus;
    int             ca;
    int             cr;
    int             crMinus;
    // Controls the weights that
    //  positive message and negative message contribute respectively
    // See nodeStateGain[] and setNodeStateGain(lambda) for details
    double          lambda;
    // Number of nodes
    std::size_t     n;
    // Number of boosted nodes to choose
    std::size_t     k;
    // The maximum number of PRR-sketch samples
    std::size_t     sampleLimit;
    // Limit of theta_sa
    std::size_t     thetaSALimit;
    // How many times to test the solution by forward simulation
    std::size_t     testTimes;
    // Used for debug logging
    double          logPerPercentage;
    // Algorithm approximation ratio = delta - epsilon_pr
    // delta = 1 - 1/e (about 0.632) by default
    // IMM algorithm returns a solution of approximation ratio (delta - epsilon_pr)
    //  with at least 1 - (1/n)^ell probability
    double          delta = 1.0 - 1.0 / ns::e;
    // Algorithm approximation ratio of random greedy = delta - epsilon_pr
    // delta = 1/e (about 0.368) by default
    // IMM algorithm with random greedy returns a solution of approximation ratio (delta - epsilon_pr)
    //  with at least 1 - (1/n)^ell probability
    double          delta_rg = 1.0 / ns::e;
    // Controls the probability of a (delta - epsilon_pr)-approximate solution
    // The higher ell, the lower probability for a low-quality result,
    //  but the higher time & space consumption
    // ell = 1.0 by default
    double          ell;
    // Controls the approximation ratio
    // The lower delta, the higher approximation ratio (i.e. higher quality result)
    //  but the higher time & space consumption
    double          epsilon_pr;
    double          epsilon_sa;
    // Minimum average gain that a node s should contribute to certain center node v
    //  during SA-IMM algorithm.
    // This threshold is set to save memory usage by ignoring nodes with too little contribution.
    double          gainThreshold_sa;

    // The following parameters are derived from those above
    // See .updateValues() for details
    double          ellP;

    double          alpha;
    double          beta;
    double          theta0_pr;
    double          kappa;
    double          kappa_rg;
    double          theta_sa;
    double          theta_sa_rg;

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

        ellP = ell * (1.0 + ns::ln2 / lnN);

        alpha = delta * std::sqrt(ellP * lnN + ns::ln2);
        beta = std::sqrt(delta * (ellP * lnN + lnCnk + ns::ln2));
        theta0_pr = (1.0 + ns::sqrt2 * epsilon_pr / 3.0)
                    * (lnCnk + ellP * lnN + std::log(log2N))
                    / std::pow(epsilon_pr, 2.0);

        auto getThetaSA = [this](double whichDelta, double whichKappa) {
            return (2.0 + 2.0 * whichKappa / 3.0)
                   * (1.0 + whichDelta + whichKappa)
                   * ((ellP + 1.0) * lnN + ns::ln2)
                   / ((2.0 + whichDelta) * std::pow(whichKappa, 3.0));
        };

        kappa = epsilon_sa / (2.0 * delta - epsilon_sa);
        kappa_rg = epsilon_sa / (2.0 * delta_rg - epsilon_sa);

        theta_sa = getThetaSA(delta, kappa);
        theta_sa_rg = getThetaSA(delta_rg, kappa_rg);
    }

    void setPriority(int _caPlus, int _ca, int _cr, int _crMinus) {
        this->caPlus = _caPlus;
        this->ca = _ca;
        this->cr = _cr;
        this->crMinus = _crMinus;
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
        priorityStr += " : " + NodePriorityProperty::of(caPlus, ca, cr, crMinus).dump();

        auto res = std::string("Arguments:\n") + doFormat({
            {"graphPath", graphPath},
            {"seedSetPath", seedSetPath},
            {"algo", toString(algo)},
            {"priority", priorityStr},
            {"lambda", toString(lambda)},
            {"epsilon_pr", toString(epsilon_pr)},
            {"epsilon_sa", toString(epsilon_sa)},
            {"ell", toString(ell)},
            {"delta", toString(delta)},
            {"delta_rg", toString(delta_rg)},
            {"gainThreshold_sa", toString(gainThreshold_sa)},
            {"n", toString(n)},
            {"k", toString(k)},
            {"sampleLimit", toString(sampleLimit)},
            {"thetaSALimit", toString(thetaSALimit)},
            {"testTimes", toString(testTimes)}
        }) + "Derived arguments:\n" + doFormat({
            {"alpha", toString(alpha)},
            {"beta", toString(beta)},
            {"theta0_pr", toString(theta0_pr)},
            {"kappa", toString(kappa)},
            {"kappa_rg", toString(kappa_rg)},
            {"theta_sa", toString(theta_sa)},
            {"theta_sa_rg", toString(theta_sa_rg)}
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
