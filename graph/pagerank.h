//
// Created by Onlynagesha on 2022/4/8.
//

#ifndef DAWNSEEKER_GRAPH_PAGERANK_H
#define DAWNSEEKER_GRAPH_PAGERANK_H

#include <algorithm>
#include <cmath>
#include <numeric>
#include "graph.h"
#include "utils/type_traits.h"

namespace graph {
    template <NodeOrIndex Node,
              LinkType Link,
              IndexMapType IndexMap,
            tags::EnablesFastAccess enablesFastAccess>
    class PageRankResult {
        using ParentType = Graph<Node, Link, IndexMap, enablesFastAccess>;

        const ParentType* _parent;
        std::vector<double> _pr;

    public:
        PageRankResult(const ParentType* parent, std::vector<double> pr):
                _parent(parent), _pr(std::move(pr)) {}

        double operator [] (const NodeOrIndex auto& node) const {
            return _pr[_parent->mappedIndex(node)];
        }

        [[nodiscard]] double sum() const {
            return std::accumulate(_pr.begin(), _pr.end(), 0.0);
        }

        [[nodiscard]] bool isNormalized(double* sumRes = nullptr, double eps = 1e-6) const {
            auto s = sum();
            if (sumRes != nullptr) {
                *sumRes = s;
            }
            return std::fabs(s - 1.0) <= eps;
        }
    };

    namespace helper {
        template <NodeOrIndex Node,
                  LinkType Link,
                  IndexMapType IndexMap,
                  tags::EnablesFastAccess enablesFastAccess>
        auto pageRank(const Graph<Node, Link, IndexMap, enablesFastAccess>& graph,
                      double alpha = 0.85,
                      double eps = 1e-6) {
            if (alpha <= 0.0 || alpha >= 1.0) {
                throw std::invalid_argument("alpha must be in (0, 1)");
            }

            auto invN = 1.0 / graph.nNodes();
            auto C = (1.0 - alpha) * invN;

            // PR[v] = page rank of node v, where v is mapped index
            // Initially PR[v] = 1.0 for each v
            auto pr = std::vector<double>(graph.nNodes(), invN);

            auto getErrorSquared = [](const auto& p1, const auto& p2) {
                double res = 0.0;
                for (std::size_t i = 0; i < p1.size(); i++) {
                    double diff = p1[i] - p2[i];
                    res += diff * diff;
                }
                return res;
            };

            auto doIteration = [&]() -> double {
                static auto nextPR = std::vector<double>();
                nextPR.assign(graph.nNodes(), 0.0);

                auto sum = 0.0;
                for (const auto& from: graph.nodes()) {
                    auto inc = pr[graph.fastMappedIndex(from)];
                    if (graph.fastOutDegree(from) == 0) {
                        sum += inc;
                        continue;
                    }
                    inc /= graph.fastOutDegree(from);
                    for (const auto& [to, _]: graph.fastLinksFrom(from)) {
                        nextPR[graph.fastMappedIndex(to)] += inc;
                    }
                }

                for (auto& v: nextPR) {
                    v = std::fma(std::fma(sum, invN, v), alpha, C);
                }

                double err = getErrorSquared(pr, nextPR);
                pr = std::move(nextPR);
                return err;
            };

            while (doIteration() >= eps * eps) {}
            return PageRankResult(&graph, std::move(pr));
        }
    }

    /*!
     * @brief Gets the page rank of each node in a directed graph
     *
     * @param graph The directed graph object. Left-value reference is required
     * @param alpha Damping factor in PageRankResult algorithm (0.85 by default)
     * @param eps Tolerance of error during iteration (1e-6 by default)
     * @return a PageRankResult object to access the page rank value of each node
     */
    template <class GraphType> requires (std::is_lvalue_reference_v<GraphType>)
    auto pageRank(GraphType&& graph, double alpha = 0.85, double eps = 1e-6) {
        return helper::pageRank(graph, alpha, eps);
    }
}

#endif //DAWNSEEKER_GRAPH_PAGERANK_H
