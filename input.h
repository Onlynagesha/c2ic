//
// Created by Onlynagesha on 2022/5/7.
//

#ifndef DAWNSEEKER_INPUT_H
#define DAWNSEEKER_INPUT_H

#include <fstream>
#include "args-v2.h"
#include "graphbasic.h"

/*!
 * @brief Reads the graph from given input stream.
 *
 * Format of input:
 *   - First line: two positive integers V, E, number of nodes and links of the graph;
 *   - Then E lines, each line contains 4 values u, v, p, pBoost
 *     indicating a directed link u -> v with probabilities p and pBoost.
 *
 * Requirements of input values:
 *   - Node indices should be in the range [0, E-1]
 *
 * @param in Input stream
 * @return The graph object
 */
inline IMMGraph readGraph(std::istream& in) {
    auto graph = IMMGraph(graph::tags::reservesLater);

    std::size_t V, E;
    in >> V >> E;
    graph.reserve({{"nodes", V}, {"links", E}});

    for (std::size_t i = 0; i < V; i++) {
        graph.fastAddNode(IMMNode(i));
    }

    std::size_t from, to;
    std::size_t index = 0;
    double p, pBoost;
    for (; in >> from >> to >> p >> pBoost; ) {
        if (from >= V || to >= V) {
            throw std::out_of_range("invalid node index: from >= V or to >= V");
        }
        graph.fastAddLink(IMMLink(from, to, index++, p, pBoost));
    }
    return graph;
}

/*!
 * @brief Reads the graph from given file path.
 *
 * See readGraph(std::istream&) for details.
 *
 * @param path Path of the input file
 * @return The graph object.
 */
inline IMMGraph readGraph(const fs::path& path) {
    auto fin = std::ifstream(path);
    if (!fin.is_open()) {
        throw std::invalid_argument("Graph file not found!");
    }
    return readGraph(fin);
}

/*!
 * @brief Reads the seed set from given input stream.
 *
 * Input format:
 *   - First line: an integer N_a, number of seeds with positive information;
 *   - Second line: N_a integers, indices of positive seed nodes;
 *   - Third line: an integer N_r, number of seeds with negative information;
 *   - Fourth line: N_r integers, indices of negative seed nodes.
 *
 * @param in Input stream
 * @return The seed set object.
 */
inline SeedSet readSeedSet(std::istream& in) {
    std::size_t Na, Nr;
    auto Sa = std::vector<std::size_t>();
    auto Sr = std::vector<std::size_t>();

    in >> Na;
    for (std::size_t v, i = 0; i < Na; in >> v, Sa.push_back(v), i++);
    in >> Nr;
    for (std::size_t v, i = 0; i < Nr; in >> v, Sr.push_back(v), i++);

    return {std::move(Sa), std::move(Sr)};
}

/*!
 * @brief Reads the seed set from the file with given path.
 *
 * See readSeedSet(std::istream& for details.
 *
 * @param path File path
 * @return The seed set object.
 */
inline SeedSet readSeedSet(const fs::path& path) {
    auto fin = std::ifstream(path);
    if (!fin.is_open()) {
        throw std::invalid_argument("Seed file not found!");
    }
    return readSeedSet(fin);
}

/*!
 * @brief An all-in-one interface to handle input.
 *
 * The function processes as the following:
 *   - Parses the program arguments (argc, argv) and wraps all the algorithm arguments into an object
 *     (see prepareProgramArgs and getAlgorithmArgs for details);
 *   - Reads the graph and seed set from given file path
 *     (see readGraph and readSeedSet for details).
 *
 * @param argc
 * @param argv
 * @return A structure as {graph, seeds, args}
 */
inline auto handleInput(int argc, char** argv) {
    struct ResultType {
        IMMGraph            graph;
        SeedSet             seeds;
        AlgorithmArgsPtr    args;
    };

    auto argSet = prepareProgramArgs(argc, argv);
    auto graph  = readGraph(argSet.s["graph-path"]);
    auto seeds  = readSeedSet(argSet.s["seed-set-path"]);
    auto args   = getAlgorithmArgs(graph.nNodes(), argSet);

    return ResultType{
        .graph = std::move(graph), // NOLINT(performance-move-const-arg)
        .seeds = std::move(seeds),
        .args  = std::move(args)
    };
}

#endif //DAWNSEEKER_INPUT_H
