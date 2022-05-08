//
// Created by Onlynagesha on 2022/5/7.
//

#ifndef DAWNSEEKER_INPUT_H
#define DAWNSEEKER_INPUT_H

#include <fstream>
#include "args-v2.h"
#include "immbasic.h"

/*
* Reads the graph.
* File format:
* The first line contains two integers V, E, denoting number of nodes and links
* Then E lines, each line contains 4 values u, v, p, pBoost
*   indicating a directed link u -> v with probabilities p and pBoost
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
    double p, pBoost;
    for (; in >> from >> to >> p >> pBoost; ) {
        if (from >= V || to >= V) {
            throw std::out_of_range("invalid node index: from >= V or to >= V");
        }
        graph.fastAddLink(IMMLink(from, to, p, pBoost));
    }
    return graph;
}

inline IMMGraph readGraph(const fs::path& path) {
    auto fin = std::ifstream(path);
    if (!fin.is_open()) {
        throw std::invalid_argument("Graph file not found!");
    }
    return readGraph(fin);
}

/*
 * Reads the seed set.
 * File format:
 * The first line contains an integer Na, number of positive seeds.
 * The next line contains Na integers, indices of the positive seeds.
 * The third line contains an integer Nr, number of negative seeds.
 * The fourth line contains Nr integers, indices of the negative seeds
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

inline SeedSet readSeedSet(const fs::path& path) {
    auto fin = std::ifstream(path);
    if (!fin.is_open()) {
        throw std::invalid_argument("Seed file not found!");
    }
    return readSeedSet(fin);
}

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
        .graph = graph,
        .seeds = std::move(seeds),
        .args  = std::move(args)
    };
}

#endif //DAWNSEEKER_INPUT_H
