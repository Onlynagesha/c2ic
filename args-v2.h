//
// Created by Onlynagesha on 2022/4/5.
//

#ifndef DAWNSEEKER_GRAPH_ARGS_V2_H
#define DAWNSEEKER_GRAPH_ARGS_V2_H

#include "argparse/argparse.hpp"
#include "args/args.h"
#include "global.h"
#include "immbasic.h"
#include "Logger.h"
#include <cmath>
#include <thread>
#include <memory>
#include <vector>

using ProgramArgs = args::CIArgSet;

enum class AlgorithmLabel {
    PR_IMM,
    SA_IMM,
    SA_RG_IMM,
    Greedy,
    MaxDegree,
    PageRank
};

/*!
 * @brief Converts an AlgorithmLabel to its corresponding name as std::string.
 * @param label 
 * @return A std::string object as its name.
 */
inline std::string toString(AlgorithmLabel label) {
    switch (label) {
    case AlgorithmLabel::PR_IMM:
        return "PR_IMM";
    case AlgorithmLabel::SA_IMM:
        return "SA_IMM";
    case AlgorithmLabel::SA_RG_IMM:
        return "SA_RG_IMM";
    case AlgorithmLabel::Greedy:
        return "Greedy";
    case AlgorithmLabel::MaxDegree:
        return "MaxDegree";
    case AlgorithmLabel::PageRank:
        return "PageRank";
    default:
        return "(ERROR)";
    }
}

/*!
 * @brief Gets the algorithm label by algorithm name and/or node state priority.
 * 
 * Algorithm name is case-insensitive. Valid algorithm names are:
 *   - `greedy`: Greedy algorithm
 *   - `max-degree` or `MaxDegree`: Max-degree algorithm, picking the nodes with the highest degree
 *   - `pagerank` or `PageRank`: PageRank algorithm, picking the nodes with the highest PageRank value
 *   - `pr-imm`: PR-IMM algorithm (for monotonic & submodular cases)
 *   - `sa-imm`: SA-IMM algorithm (for monotonic & non-submodular cases)
 *   - `sa-rg-imm`: SA-RG-IMM algorithm (for non-monotonic & non-submodular cases)
 *   - `auto`: One of PR-IMM, SA-IMM or SA-RG-IMM according to node state priority.
 *   
 * An exception is thrown if algorithm name does not match any of them above. 
 * 
 * @param algo Case-insensitive string as the algorithm name.
 * @param priority Node state priority.
 * @return The corresponding AlgorithmLabel
 * @throw std::invalid_argument if no valid algorithm label is matched.
 */
inline AlgorithmLabel getAlgorithmLabel(const utils::ci_string& algo, const NodePriorityProperty& priority) {
    if (algo == "greedy") {
        return AlgorithmLabel::Greedy;
    }
    if (algo == "max-degree" || algo == "maxDegree") {
        return AlgorithmLabel::MaxDegree;
    }
    if (algo == "page-rank" || algo == "pagerank") {
        return AlgorithmLabel::PageRank;
    }
    if (algo == "pr-imm") {
        return AlgorithmLabel::PR_IMM;
    }
    if (algo == "sa-imm") {
        return AlgorithmLabel::SA_IMM;
    }
    if (algo == "sa-rg-imm") {
        return AlgorithmLabel::SA_RG_IMM;
    }
    
    if (algo != "auto") {
        throw std::invalid_argument("No matching algorithm");
    }

    if (priority.satisfies("M - S")) {
        return AlgorithmLabel::PR_IMM;
    }
    if (priority.satisfies("M - nS")) {
        return AlgorithmLabel::SA_IMM;
    }
    if (priority.satisfies("nM - nS")) {
        return AlgorithmLabel::SA_RG_IMM;
    }

    throw std::invalid_argument("No matching algorithm");
}

/*!
 * @brief Gets the algorithm label by algorithm name and/or node state priority.
 * 
 * args should contain:
 *   - args["priority"] as case-insensitive string: string representation of node state priority.
 *   - (Optional) args["algo"] as case-insensitive string: algorithm name. `auto` by default.
 *   
 * See getAlgorithmLabel(algo, priority) for details.
 * 
 * @param args The ProgramArgs object as argument collection.
 * @return The corresponding AlgorithmLabel
 */
inline AlgorithmLabel getAlgorithmLabel(const ProgramArgs& args) {
    return getAlgorithmLabel(
            args.getValueOr("algo", utils::ci_string("auto")),
            NodePriorityProperty::of(args["priority"].get<utils::ci_string>())
            );
}

/*!
 * @brief Checks whether the argument collection provides fixed sample size for given algorithm.
 *
 * For PR-IMM algorithm, checks on args["n-samples"]:
 *   - Whether this argument exists
 *   - Whether it contains a positive integer, or a list of positive integers.
 *
 * For SA-IMM and SA-RG-IMM algorithm, checks on args["n-samples-sa"] instead.
 *
 * An exception is thrown if the algorithm label is not one of above.
 *
 * @param args The ProgramArgs object as argument collection
 * @param algo The algorithm label
 * @return The check result, whether such sample size is fixed.
 * @throw std::invalid_argument if invalid algorithm label is provided.
 */
inline bool sampleSizeIsFixed(const ProgramArgs& args, AlgorithmLabel algo) {
    auto checkFn = [&](const char* label) {
        return args.getValueOr<utils::ci_string>(label, "0") != "0";
    };

    if (algo == AlgorithmLabel::PR_IMM) {
        return checkFn("n-samples");
    }
    if (algo == AlgorithmLabel::SA_IMM || algo == AlgorithmLabel::SA_RG_IMM) {
        return checkFn("n-samples-sa");
    }
    throw std::invalid_argument("Unsupported algorithm label");
}

/*!
 * @brief whether the argument collection provides fixed sample size.
 *
 * The algorithm label is obtained via algo = getAlgorithmLabel(args).
 * See getAlgorithmLabel and sampleSizeIsFixed(args, algo) for details.
 *
 * @param args The ProgramArgs object as argument collection
 * @return The check result, whether such sample size is fixed.
 * @throw std::invalid_argument if invalid algorithm label is provided by args.
 */
inline bool sampleSizeIsFixed(const ProgramArgs& args) {
    return sampleSizeIsFixed(args, getAlgorithmLabel(args));
}

namespace {
    // Delimiter between two dump segments. See dump() of BasicArgs and its derived types for details.
    constexpr const char* dumpDelimiter  = "\n--------------------------------\n";
    // Supports space, comma or semicolon as delimiter characters
    constexpr const char* listDelimiters = " ,;";
    // Unified join function for lists in the arguments
    inline std::string listJoinFn(rs::range auto&& list) {
        return utils::join(list, ", ", "[", "]");
    }
}

/*!
 * @brief Common algorithm arguments that all the algorithms use.
 */
struct BasicArgs {
    // Approximation ratio = 1 - 1/e
    static constexpr double delta   = 1.0 - 1.0 /  ns::e;
    // Approximation ratio of random greedy algorithm = 1/e
    static constexpr double deltaRG = 1.0 / ns::e;

    // Number of nodes in the graph
    std::size_t                 n;
    // Number of boosted nodes to choose
    // If multiple k's are provided, take the largest k.
    std::size_t                 k;
    // All the k's for simulation
    std::vector<std::size_t>    kList;
    // Weight of objective function. 
    // The higher lambda, the result depends more on positive gain;
    // The lower  lambda,                    more on negative gain. 
    double                      lambda;
    // Node state priority
    NodePriorityProperty        priority{};
    // Algorithm label
    AlgorithmLabel              algo;
    // Logging frequency during time-consuming algorithms.
    double                      logPerPercentage;
    // Number of threads to use
    std::size_t                 nThreads;
    // Number of simulations for each boosted node set and each k
    static constexpr std::uint64_t  testTimesDefault = 10000;
    std::uint64_t                   testTimes;

    // Derived Args
    double                  log2N;
    double                  lnN;
    double                  lnCnk;

    /*!
     * @brief Constructs a BasicArgs object with given graph size and argument collection.
     * 
     * args should contain:
     *   - args["k"]                             as unsigned integer, or a list of unsigned integers,
     *                                              number of boosted nodes to choose.
     *   - args["lambda"]                        as floating point, 
     *                                              weight of objective function
     *   - args["priority"]                      as case-insensitive string,
     *                                              string representation of node state priority
     *   - (Optional) args["algo"]               as case-insensitive string, 
     *                                              algorithm name. `auto` by default
     *   - (Optional) args["log-per-percentage"] as floating point, 
     *                                              log frequency. 5.0 by default
     *   - (Optional) args["n-threads"]          as unsigned integer,
     *                                              how many threads to use at most, 1 by default
     *   - (Optional) args["test-times"]         as unsigned integer,
     *                                              how many times to simulate for each boosted node set and k
     *
     * For the arguments that support lists, the values should be separated by spaces, commas or semicolons.
     * e.g. "10, 20; 30"
     * 
     * @param n Number of nodes in the whole graph
     * @param args The ProgramArgs object as argument collection.
     * @throw std::out_of_range if value gets out of range
     */
    BasicArgs(std::size_t n, const ProgramArgs& args): n(n) {
        if (n <= 1) {
            throw std::out_of_range("n >= 2 is not satisfied");
        }

        auto kStr = args["k"].get<utils::ci_string>();
        utils::cstr::splitByEither(kStr.data(), kStr.length(), listDelimiters, [&](std::string_view token) {
            kList.push_back(utils::fromString<std::size_t>(token));
        });

        if (kList.empty()) {
            throw std::out_of_range("List of k is empty");
        }
        for (auto kx: kList) {
            if (kx > n) {
                throw std::out_of_range("k <= n is not satisfied");
            }
        }
        // Sorts in ascending order
        rs::sort(kList);
        // Takes the largest k
        k = kList.back();

        lambda = args["lambda"].get<double>();
        if (lambda < 0.0 || lambda > 1.0) {
            throw std::out_of_range("0 <= lambda <= 1 is not satisfied");
        }

        priority = NodePriorityProperty::of(args["priority"].get<utils::ci_string>());
        algo = getAlgorithmLabel(args.getValueOr("algo", utils::ci_string("auto")), priority);
        
        logPerPercentage = args.getValueOr("log-per-percentage", 5.0);
        if (logPerPercentage <= 0.0) {
            throw std::out_of_range("logPerPercentage > 0 is not satisfied");
        }
        if (logPerPercentage > 100.0) {
            logPerPercentage = 100.0;
            LOG_WARNING("logPerPercentage <= 100 is not satisfied. Sets to 100.");
        }

        nThreads = args.getValueOr("n-threads", std::size_t{1});
        if (nThreads == 0) {
            nThreads = 1;
            LOG_WARNING("nThreads >= 1 is not satisfied. Sets to 1.");
        }
        if (nThreads > std::thread::hardware_concurrency()) {
            nThreads = std::thread::hardware_concurrency();
            LOG_WARNING(format("nThreads <= {0} is not satisfied. Sets to {0}.",
                               std::thread::hardware_concurrency()));
        }

        testTimes = args.getValueOr("test-times", testTimesDefault);
        if (testTimes == 0) {
            testTimes = testTimesDefault;
            LOG_WARNING(format("testTimes >= 1 is not satisfied. Sets to {}.", testTimesDefault));
        }

        log2N = std::log2(n);
        lnN = std::log(n);

        lnCnk = 0.0;
        for (auto x = n - k + 1; x <= n; x++) {
            lnCnk += std::log(x);
        }
        for (auto x = 2; x <= k; x++) {
            lnCnk -= std::log(x);
        }
    }

    virtual ~BasicArgs() = default;

    /*!
     * @brief Sets global experiment environment.
     *
     * Changes the following:
     *   - Global weight parameter lambda
     *   - Global node state priority
     */
    virtual void setEnv() const {
        setNodeStateGain(lambda);
        setNodeStatePriority(priority.array);
    }

    /*!
     * @brief Dumps arguments as a string.
     * @return A multi-line string, without trailing new-line character.
     */
    [[nodiscard]] virtual std::string dump() const {
        auto res = format("               n = {}\n", n);
        res     += format("=>         log2N = {}\n", log2N);
        res     += format("=>           lnN = {}\n", lnN);
        res     += format("           kList = {}\n", listJoinFn(kList));
        res     += format("               k = {}\n", k);
        res     += format("=>         lnCnk = {}\n", lnCnk);
        res     += format("          lambda = {}\n", lambda);
        res     += format("            algo = {}\n", algo);
        res     += format("logPerPercentage = {}\n", logPerPercentage);
        res     += format("        nThreads = {}\n", nThreads);
        res     += format("       testTimes = {} (default = {})\n", testTimes, testTimesDefault);
        
        res     += format("Node state priority: \n{}", priority.dump());
        return res;
    }
};

/*!
 * @brief Common algorithm arguments for SA-IMM-LB and SA-RG-IMM-LB algorithms.
 *
 * Suffix -LB refers to the lower bound part.
 * This derived class contains additionally the common arguments
 *  that these lower bound fractions require.
 */
struct BasicArgs_SA_IMM_LB: BasicArgs {
    // Constraints the threshold of minimal distance between the center node of a sample and any of seed nodes.
    // Only the nodes with distance in the range [1, sampleDistLimit] are chosen as center nodes.
    // This parameter saves time consumption with slight solution quality loss if the graph is very large.
    // +inf by default, with which all the nodes will be chosen.
    std::size_t sampleDistLimit;
    // Minimum average gain that a boosted node s should contribute to certain center node v
    //  in SA-IMM or SA-RG-IMM algorithm.
    // Only the records that satisfies gain(s; v) >= gainThreshold will be stored. Others filtered out.
    // See SA_IMM_LB for details.
    double      gainThreshold;

    /*!
     * @brief Constructs with given graph size and argument collection.
     *
     * Besides requirements in BasicArgs and K, args should also contain:
     *   - args["sample-dist-limit-sa"]         as an unsigned integer,
     *                                              the maximum minimal distance
     *                                              between a sample's center node
     *                                              and any of seed nodes
     *   - (Optional) args["gain-threshold-sa"] as a floating point,
     *                                              gain threshold, 0.0 by default
     *
     * @param n Number of nodes in the whole graph
     * @param args The ProgramArgs object as argument collection.
     * @throw std::out_of_range if value gets out of range
     */
    BasicArgs_SA_IMM_LB(std::size_t n, const ProgramArgs& args): BasicArgs(n, args) {
        sampleDistLimit = args.getValueOr("sample-dist-limit-sa", utils::halfMax<std::size_t>);
        if (sampleDistLimit == 0) {
            throw std::out_of_range("sampleDistLimit >= 1 is not satisfied");
        }

        gainThreshold = args.getValueOr("gain-threshold-sa", 0.0);
        if (gainThreshold >= 1.0) {
            throw std::out_of_range("gainThreshold < 1 is not satisfied");
        }
        if (gainThreshold < 0.0) {
            gainThreshold = 0.0;
            LOG_WARNING("gainThreshold >= 0.0 is not satisfied. Sets to 0.");
        }
    }

    /*!
     * @brief Dumps arguments as a string.
     * @return A multi-line string, without trailing new-line character.
     */
    [[nodiscard]] std::string dump() const override {
        auto res = BasicArgs::dump() + dumpDelimiter;
        res += format("sampleDistLimit = {}\n", sampleDistLimit);
        res += format("  gainThreshold = {}",   gainThreshold);
        return res;
    }
};

/*!
 * @brief Abstract base class for all argument components on sample size.
 */
struct BasicArgsSampleSize {
    /*!
     * @brief Virtual destruction for derived types.
     */
    virtual ~BasicArgsSampleSize() = default;

    /*!
     * @brief Gets the information whether the number of PRR-sketch samples is fixed at constant value(s).
     * @return True if fixed, false otherwise.
     */
    [[nodiscard]] virtual bool isFixed() const = 0;

    /*!
     * @brief Interface to dump arguments as a string.
     * @return A multi-line string, without trailing new-line character.
     */
    [[nodiscard]] virtual std::string dump() const = 0;
};

/*!
 * @brief Common algorithm arguments for dynamic cases.
 *
 * The number of samples is determined by epsilon, ell and sampleLimit.
 */
struct ArgsSampleSizeDynamic: BasicArgsSampleSize {
    // Epsilon.
    double          epsilon;
    // Ell. With 1 - (1/n) ^ ell probability,
    // PR-IMM    algorithm gets a result with approximation ratio (1 - 1/e - epsilon),
    // SA-IMM    algorithm gets a result with approximation ratio (1 - 1/e - epsilon),
    // SA-RG-IMM                                                      (1/e - epsilon).
    double          ell;
    // Maximum number of samples.
    // For PR-IMM algorithm:
    //   sampleLimit = The maximum total number of samples.
    //     The algorithm is forced to stop if the number of PRR-sketches reaches this limit.
    // For SA-IMM and SA-RG-IMM algorithm:
    //   sampleLimit = The maximum number of samples for each center node.
    //     The total sample size = min(sampleLimit, theta(epsilon, ell)) * N'
    //     where N' = number of center nodes. N' < |V| if sampleDistLimit < +inf
    std::uint64_t   sampleLimit;

    /*!
     * @brief Constructs with given algorithm label and argument collection.
     *
     * For PR-IMM algorithm, args should contain:
     *   - args["epsilon"]                  as floating point
     *   - (Optional) args["ell"]           as floating point, 1.0 by default
     *   - (Optional) args["sample-limit"]  as floating point, +inf by default
     *
     * For SA-IMM or SA-RG-IMM algorithm, args should contain:
     *   - args["epsilon-sa"]                  as floating point
     *   - (Optional) args["ell"]              as floating point, 1.0 by default
     *   - (Optional) args["sample-limit-sa"]  as floating point, +inf by default
     *
     * @param algo Label of the target algorithm
     * @param args The ProgramArgs object as argument collection.
     * @throw std::out_of_range if value gets out of range
     */
    explicit ArgsSampleSizeDynamic(AlgorithmLabel algo, const ProgramArgs& args) {
        epsilon = args[algo == AlgorithmLabel::PR_IMM ? "epsilon" : "epsilon-sa"].get<double>();
        if (epsilon <= 0.0 || epsilon >= 1.0) {
            throw std::out_of_range("0 < delta < 1 is not satisfied");
        }

        ell = args.getValueOr("ell", 1.0);
        if (ell <= 0.0) {
            throw std::out_of_range("ell > 0 is not satisfied");
        }

        sampleLimit = args.getValueOr(
                algo == AlgorithmLabel::PR_IMM ? "sample-limit" : "sample-limit-sa",
                utils::halfMax<std::uint64_t>);
    }

    /*!
     * @brief Gets the information whether the number of PRR-sketch samples is fixed at constant value(s).
     *
     * Always false for dynamic cases, whose sample size is determined by epsilon, ell and sample-limit
     *  and early-stop process in the algorithm.
     *
     * @return false
     */
    [[nodiscard]] bool isFixed() const override {
        return false;
    }

    /*!
     * @brief Dumps arguments as a string.
     * @return A multi-line string, without trailing new-line character.
     */
    [[nodiscard]] std::string dump() const override {
        auto res = format("    epsilon = {}\n", epsilon);
        res     += format("        ell = {}\n", ell);
        res     += format("sampleLimit = {}",   sampleLimit);
        return res;
    }
};

/*!
 * @brief Common algorithm arguments for static cases.
 *
 * The number of PRR-sketches in total (PR-IMM),
 *  or for each center node (lower bound part of SA-[RG-]IMM),
 *  are provided as a list of integers.
 * Different results are gotten for each sample size.
 * After generating R1 samples, the result with R1 + R2 samples is acquired by simply appending R2 samples.
 */
struct ArgsSampleSizeStatic: BasicArgsSampleSize {
    // For PR-IMM:
    //   List of all the sample sizes (i.e. number of PRR-sketches)
    // For SA-IMM and SA-RG-IMM (lower bound part):
    //   List of all the sample sizes for each center node.
    // Sorted in ascending order
    std::vector<std::uint64_t> nSamplesList;

    /*!
     * @brief Constructs with given algorithm label and argument collection.
     *
     * For PR-IMM algorithm, args should contain:
     *   - args["n-samples"] as a case-sensitive string,
     *      a list of all the sample sizes, separated with either space, comma or semicolon.
     *
     * For SA-IMM and SA-RG-IMM algorithm (lower bound part), use args["n-samples-sa"] instead.
     *
     * @param algo Label of the target algorithm
     * @param args The ProgramArgs object as argument collection.
     * @throw std::out_of_range if value gets out of range
     */
    ArgsSampleSizeStatic(AlgorithmLabel algo, const ProgramArgs& args) {
        auto listStr = args[algo == AlgorithmLabel::PR_IMM ? "n-samples" : "n-samples-sa"].get<std::string>();
        utils::cstr::splitByEither(listStr.data(), listStr.length(), listDelimiters, [&](std::string_view token) {
            nSamplesList.push_back(utils::fromString<std::uint64_t>(token));
        });
        // Sorts in ascending order
        rs::sort(nSamplesList);
    }

    /*!
     * @brief Constructs with a single sample size value.
     * @param nSamples
     */
    explicit ArgsSampleSizeStatic(std::uint64_t nSamples): nSamplesList({nSamples}) {}

    /*!
     * @brief Constructs with a list of sample sizes.
     * @param range A range object of the sample sizes.
     */
    template <rs::range Range>
    requires (std::convertible_to<rs::range_value_t<Range>, std::uint64_t>)
    explicit ArgsSampleSizeStatic(Range&& range) { // NOLINT(bugprone-forwarding-reference-overload)
        auto cm = range | vs::common;
        nSamplesList.assign(cm.begin(), cm.end());
        rs::sort(nSamplesList);
    }

    /*!
     * @brief Gets the information whether the number of PRR-sketch samples is fixed at constant value(s).
     *
     * Always true for static cases, whose sample size is given explicitly.
     *
     * @return true
     */
    [[nodiscard]] bool isFixed() const override {
        return true;
    }

    /*!
     * @brief Dumps arguments as a string.
     * @return A multi-line string, without trailing new-line character.
     */
    [[nodiscard]] std::string dump() const override {
        return "nSamples = " + listJoinFn(nSamplesList);
    }
};

/*!
 * @brief Derived component of PR-IMM algorithm arguments with dynamic sample size.
 */
struct ArgsSampleSizeDynamic_PR_IMM: ArgsSampleSizeDynamic {
    // Derived args (see PR_IMM for details)
    double theta0;
    double alpha;
    double beta;

    /*!
     * @brief Constructs with given base component and argument collection.
     *
     * args should meet all the requirements of ArgsSampleSizeDynamic.
     *
     * @param parent Pointer to the base component
     * @param args The ProgramArgs object as argument collection.
     * @throw std::out_of_range if value gets out of range
     */
    ArgsSampleSizeDynamic_PR_IMM(const BasicArgs* parent, const ProgramArgs& args):
            ArgsSampleSizeDynamic(AlgorithmLabel::PR_IMM, args) {
        constexpr auto delta = BasicArgs::delta;

        theta0 = (1.0 + ns::sqrt2 * epsilon / 3.0) *
                 (parent->lnCnk + ell * parent->lnN + std::log(parent->log2N)) /
                 std::pow(epsilon, 2.0);
        alpha = delta * std::sqrt(ell * parent->lnN + ns::ln2);
        beta = std::sqrt(delta * (ell * parent->lnN + parent->lnCnk + ns::ln2));
    }

    /*!
     * @brief Dumps arguments as a string.
     * @return A multi-line string, without trailing new-line character.
     */
    [[nodiscard]] std::string dump() const override {
        auto res = ArgsSampleSizeDynamic::dump() + dumpDelimiter;
        res     += format("=> theta0 = {}\n", theta0);
        res     += format("=>  alpha = {}\n", alpha);
        res     += format("=>   beta = {}", beta);
        return res;
    }
};

/*!
 * @brief Derived component of SA-IMM-LB and SA-RG-IMM-LB algorithm arguments with dynamic sample size.
 *
 * Suffix -LB refers to the lower bound part.
 *
 * Calculation of the derived arguments uses different delta:
 *   - 1 - 1/e for SA-IMM-LB
 *   -     1/e for SA-RG-IMM-LB
 */
struct ArgsSampleSizeDynamic_SA_IMM_LB: ArgsSampleSizeDynamic {
    // Derived args
    double          kappa;
    double          theta;

    /*!
     * @brief Constructs with given base component and argument collection.
     *
     * Calculation of derived arguments used different delta depending on which algorithm is used
     * (1 - 1/e for SA-IMM and 1/e for SA-RG-IMM).
     *
     * args should meet all the requirements of ArgsSampleSizeDynamic.
     *
     * @param parent Pointer to the base component
     * @param args The ProgramArgs object as argument collection.
     * @throw std::out_of_range if value gets out of range
     */
    ArgsSampleSizeDynamic_SA_IMM_LB(const BasicArgs* parent, const ProgramArgs& args):
            ArgsSampleSizeDynamic(parent->algo, args) {
        double deltaUsed = (parent->algo == AlgorithmLabel::SA_RG_IMM)
                ? BasicArgs::deltaRG
                : BasicArgs::delta;

        if (2.0 * deltaUsed - epsilon <= 0.0) {
            throw std::out_of_range("epsilon < 2 * delta is not satisfied");
        }

        kappa = epsilon / (2.0 * deltaUsed - epsilon);
        theta = (2.0 + 2.0 * kappa / 3.0) *
                (1.0 + deltaUsed + kappa) *
                ((ell + 1.0) * parent->lnN + ns::ln2) /
                ((2.0 + deltaUsed) * std::pow(kappa, 3.0));
    }

    /*!
     * @brief Dumps arguments as a string.
     * @return A multi-line string, without trailing new-line character.
     */
    [[nodiscard]] std::string dump() const override {
        auto res = ArgsSampleSizeDynamic::dump() + dumpDelimiter;
        res     += format("=> kappa = {}\n", kappa);
        res     += format("=> theta = {}", theta);
        return res;
    }
};

/*!
 * @brief Algorithm arguments for PR-IMM, SA-IMM-LB and SA-RG-IMM-LB algorithms, with static sample sizes.
 *
 * The suffix -LB refers to lower bound part.
 */
template <std::derived_from<BasicArgs> Base>
struct StaticArgs: Base, ArgsSampleSizeStatic {
    /*!
     * @brief Constructs with given graph size and argument collection.
     *
     * args should meet all the requirements of BasicArgsMultipleK and ArgsSampleSizeStatic.
     *
     * @param n Number of nodes in the whole graph
     * @param args The ProgramArgs object as argument collection.
     * @throw std::out_of_range if value gets out of range
     */
    StaticArgs(std::size_t n, const ProgramArgs& args):
    Base(n, args), ArgsSampleSizeStatic(AlgorithmLabel::PR_IMM, args) {}

    /*!
     * @brief Constructs by copying the two components in the base classes
     * @param base The Base component to be copied
     * @param sampleSizeStatic The ArgsSampleSizeStatic component to be copied
     */
    StaticArgs(const Base& base, const ArgsSampleSizeStatic& sampleSizeStatic):
    Base(base), ArgsSampleSizeStatic(sampleSizeStatic) {}

    /*!
     * @brief Dumps arguments as a string.
     * @return A multi-line string, without trailing new-line character.
     */
    [[nodiscard]] std::string dump() const override {
        return Base::dump() + dumpDelimiter + ArgsSampleSizeStatic::dump();
    }
};

/*!
 * @brief Algorithm arguments for PR-IMM algorithm, with static sample sizes.
 */
using StaticArgs_PR_IMM    = StaticArgs<BasicArgs>;

/*!
 * @brief Algorithm arguments for lower bound part of SA-IMM and SA-RG-IMM algorithms, with static sample sizes.
 */
using StaticArgs_SA_IMM_LB = StaticArgs<BasicArgs_SA_IMM_LB>;

/*!
 * @brief Algorithm arguments with dynamic sample sizes.
 */
template <std::derived_from<BasicArgs> Base, std::derived_from<BasicArgsSampleSize> SampleSizeComponent>
struct DynamicArgs: Base, SampleSizeComponent {
    /*!
     * @brief Constructs with given graph size and argument collection.
     *
     * args should meet all the requirements of BasicArgsSingleK and SampleSizeComponent.
     *
     * @param n Number of nodes in the whole graph
     * @param args The ProgramArgs object as argument collection.
     * @throw std::out_of_range if value gets out of range
     */
    DynamicArgs(std::size_t n, const ProgramArgs& args):
    Base(n, args), SampleSizeComponent(this, args) {}

    /*!
     * @brief Constructs by copying the two components in the base classes
     * @param base The Base component to be copied
     * @param sampleSize The SampleSizeComponent object to be copied
     */
    DynamicArgs(const Base& base, const SampleSizeComponent& sampleSize):
    Base(base), SampleSizeComponent(sampleSize) {}

    /*!
     * @brief Dumps arguments as a string.
     * @return A multi-line string, without trailing new-line character.
     */
    [[nodiscard]] std::string dump() const override {
        return Base::dump() + dumpDelimiter + SampleSizeComponent::dump();
    }
};

/*!
 * @brief Algorithm arguments for PR-IMM algorithm, with dynamic sample size.
 */
using DynamicArgs_PR_IMM = DynamicArgs<BasicArgs, ArgsSampleSizeDynamic_PR_IMM>;

/*!
 * @brief Algorithm arguments for lower bound part of SA-IMM and SA-RG-IMM algorithms, with dynamic sample size.
 */
using DynamicArgs_SA_IMM_LB = DynamicArgs<BasicArgs_SA_IMM_LB, ArgsSampleSizeDynamic_SA_IMM_LB>;

/*!
 * @brief Algorithm arguments for SA-IMM and SA-RG-IMM algorithms.
 *
 * Each of lower bound (LB) and upper bound (UB) may be either dynamic (sample size depends on epsilon etc.)
 * or static (sample sizes provided explicitly).
 */
struct Args_SA_IMM: BasicArgs_SA_IMM_LB {
    using Base          = BasicArgs_SA_IMM_LB;
    using SampleSizePtr = std::shared_ptr<BasicArgsSampleSize>;

    // Args for upper bound, performing PR-IMM algorithm with a monotonic & submodular node state priority
    SampleSizePtr UB;
    // Args for lower bound, performing SA-IMM-LB algorithm segment
    SampleSizePtr LB;

    /*!
     * @brief Constructs with given graph size and argument collection.
     *
     * args should meet all the requirements of BasicArgs_SA_IMM_LB<BasicArgsSingleK>
     * and ArgsSampleSize[Static | Dynamic_PR_IMM | Dynamic_SA_IMM_LB] depending on whether the
     * corresponding sample size shall be fixed.
     *
     * @param n Number of nodes in the whole graph
     * @param args The ProgramArgs object as argument collection.
     * @throw std::out_of_range if value gets out of range
     */
    Args_SA_IMM(std::size_t n, const ProgramArgs& args): Base(n, args) {
        if (sampleSizeIsFixed(args, AlgorithmLabel::PR_IMM)) {
            UB = std::make_unique<ArgsSampleSizeStatic>(AlgorithmLabel::PR_IMM, args);
        } else {
            UB = std::make_unique<ArgsSampleSizeDynamic_PR_IMM>(this, args);
        }

        if (sampleSizeIsFixed(args, algo)) {
            LB = std::make_unique<ArgsSampleSizeStatic>(algo, args);
        } else {
            LB = std::make_unique<ArgsSampleSizeDynamic_SA_IMM_LB>(this, args);
        }
    }

    /*!
     * @brief Gets a full copy of the arguments for the upper bound part.
     *
     * The result is either DynamicArgs_PR_IMM for dynamic sample size, or StaticArgs_PR_IMM for the static.
     *
     * The node state priority is replaced to the "upper bound priority" for this PR-IMM fraction.
     * See NodePriorityProperty::upperBound() for details.
     *
     * @return A shared pointer to the argument object copied.
     * @throw std::bad_cast for unexpected cases.
     */
    [[nodiscard]] auto argsUB() const {
        auto res = std::shared_ptr<BasicArgs>{};

        if (auto p1 = std::dynamic_pointer_cast<ArgsSampleSizeDynamic_PR_IMM>(UB); p1 != nullptr) {
            res = std::make_shared<DynamicArgs_PR_IMM>(*this, *p1);
        } else if (auto p2 = std::dynamic_pointer_cast<ArgsSampleSizeStatic>(UB); p2 != nullptr) {
            res = std::make_shared<StaticArgs_PR_IMM>(*this, *p2);
        } else {
            throw std::bad_cast();
        }

        // Replaces node state priority
        res->priority = NodePriorityProperty::upperBound();
        return res;
    }

    /*!
     * @brief Gets a full copy of the arguments for the lower bound part.
     *
     * The result is either DynamicArgs_SA_IMM_LB for dynamic sample size, or StaticArgs_SA_IMM_LB for the static.
     *
     * todo WORKAROUND: nThreads is set to 1 to prevent multithreading bug
     *
     * @return A shared pointer to the argument object copied.
     * @throw std::bad_cast for unexpected cases.
     */
    [[nodiscard]] auto argsLB() const {
        auto res = std::shared_ptr<BasicArgs>();

        if (auto p1 = std::dynamic_pointer_cast<ArgsSampleSizeDynamic_SA_IMM_LB>(LB); p1 != nullptr) {
            res = std::make_shared<DynamicArgs_SA_IMM_LB>(*this, *p1);
        } else if (auto p2 = std::dynamic_pointer_cast<ArgsSampleSizeStatic>(LB); p2 != nullptr) {
            res = std::make_shared<StaticArgs_SA_IMM_LB>(*this, *p2);
        } else {
            throw std::bad_cast();
        }

        // todo WORKAROUND: nThreads is set to 1 to prevent multithreading bug
        res->nThreads = 1;
        return res;
    }

    /*!
     * @brief Dumps arguments as a string.
     * @return A multi-line string, without trailing new-line character.
     */
    [[nodiscard]] std::string dump() const override {
        auto res = Base::dump() + dumpDelimiter;
        res += format("Args for upper bound ({}):\n", UB->isFixed() ? "static" : "dynamic")
                + UB->dump() + dumpDelimiter;
        res += format("Args for lower bound ({}):\n", LB->isFixed() ? "static" : "dynamic")
                + LB->dump();
        return res;
    }
};

/*!
 * @brief Algorithm arguments for greedy selection.
 */
struct GreedyArgs: BasicArgs {
    // How many times to simulate for each boosted node during greedy selection
    static constexpr std::uint64_t  greedyTestTimesDefault = 1000;
    std::uint64_t                   greedyTestTimes;

    /*!
     * @brief Constructs with given graph size and argument collection.
     *
     * args should meet all the requirements of BasicArgsMultipleK.
     *
     * @param n Number of nodes in the whole graph
     * @param args The ProgramArgs object as argument collection.
     * @throw std::out_of_range if value gets out of range
     */
    GreedyArgs(std::size_t n, const ProgramArgs& args): BasicArgs(n, args) {
        greedyTestTimes = args.getValueOr("greedy-test-times", greedyTestTimesDefault);
        if (greedyTestTimes == 0) {
            greedyTestTimes = greedyTestTimesDefault;
            LOG_WARNING(format("greedyTestTimes >= 1 is not satisfied. Sets to {}.", greedyTestTimesDefault));
        }
    }

    /*!
     * @brief Dumps arguments as a string.
     * @return A multi-line string, without trailing new-line character.
     */
    [[nodiscard]] std::string dump() const override {
        auto res = BasicArgs::dump() + dumpDelimiter;
        res += format("greedyTestTimes = {}", greedyTestTimes);
        return res;
    }
};

using AlgorithmArgsPtr = std::unique_ptr<BasicArgs>;

/*!
 * @brief Generates a ProgramArgs object with the program arguments (argc, argv).
 * @param argc
 * @param argv
 * @return The ProgramArgs object that wraps all the program arguments.
 */
ProgramArgs prepareProgramArgs(int argc, char** argv);

/*!
 * @brief Generates a algorithm argument object with given graph size |V| and program args.
 *
 * A polymorphic object is obtained according to which algorithm to use:
 *   - For PR-IMM algorithm, DynamicArgs_PR_IMM or StaticArgs_PR_IMM is obtained depending on which mode is used.
 *     The latter is used if the program argument '-n-samples' is provided.
 *     Otherwise, '-epsilon' should be provided.
 *   - For SA-(RG-)IMM algorithm, its arguments contain two parts:
 *     - Upper bound: [Dynamic|Static]Args_PR_IMM (depending on whether '-n-samples' is provided)
 *     - Lower bound: [Dynamic|Static]Args_SA_IMM_LB. (depending on whether '-n-samples-sa' is provided).
 *       For the dynamic case, '-epsilon-sa' shall be provided.
 *   - For Greedy algorithm: GreedyArgs
 *   - For others: BasicArgs
 *
 * @param n Graph size |V|
 * @param args The program arguments, see prepareProgramArgs(argc, argv) for details.
 * @return A smart pointer to the allocated polymorphic object with all the algorithm arguments.
 */
AlgorithmArgsPtr getAlgorithmArgs(std::size_t n, const ProgramArgs& args);

#endif //DAWNSEEKER_GRAPH_ARGS_V2_H
