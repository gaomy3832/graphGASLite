#ifndef ALGO_KERNELS_EDGE_CENTRIC_PAGERANK_PAGERANK_H_
#define ALGO_KERNELS_EDGE_CENTRIC_PAGERANK_PAGERANK_H_

#include "graph.h"
#include "algo_kernel.h"

/*
 * Graph types definitions.
 */
struct PageRankData {
    GraphGASLite::DegreeCount collected;
    double sum;
    double rank;

    PageRankData(const GraphGASLite::VertexIdx&)
        : collected(0), sum(0), rank(0)
    {
        // Nothing else to do.
    }
};

struct PageRankUpdate {
    double contribute;
    GraphGASLite::DegreeCount count;

    PageRankUpdate(const double contribute_ = 0, const GraphGASLite::DegreeCount& count_ = 0)
        : contribute(contribute_), count(count_)
    {
        // Nothing else to do.
    }

    PageRankUpdate& operator+=(const PageRankUpdate& update) {
        contribute += update.contribute;
        count += update.count;
        return *this;
    }
};


/*
 * Algorithm kernel definition.
 */
template<typename GraphTileType>
class PageRankEdgeCentricAlgoKernel : public GraphGASLite::EdgeCentricAlgoKernel<GraphTileType> {
public:
    static Ptr<PageRankEdgeCentricAlgoKernel> instanceNew(const string& name,
            const double beta, const double tolerance) {
        return Ptr<PageRankEdgeCentricAlgoKernel>(new PageRankEdgeCentricAlgoKernel(name, beta, tolerance));
    }

protected:
    typedef typename GraphTileType::UpdateType UpdateType;
    typedef typename GraphTileType::VertexType VertexType;
    typedef typename GraphTileType::EdgeType::WeightType EdgeWeightType;

    std::pair<UpdateType, bool> scatter(const GraphGASLite::IterCount&, Ptr<VertexType>& src, EdgeWeightType&) const {
        auto& data = src->data();
        auto odeg = src->outDeg();
        auto contribute = data.rank / odeg;
        std::pair<UpdateType, bool> ret;
        ret.first = PageRankUpdate(contribute, 1);
        ret.second = true;
        return ret;
    }

    bool gather(const GraphGASLite::IterCount&, Ptr<VertexType>& dst, const UpdateType& update) const {
        auto& data = dst->data();
        data.sum += update.contribute;
        data.collected += update.count;
        if (data.collected == dst->inDeg()) {
            double newRank = beta_ * data.sum + (1 - beta_);
            bool converge = (std::abs(newRank - data.rank) <= tolerance_);
            data.rank = newRank;
            data.sum = 0;
            data.collected = 0;
            return converge;
        }
        // Convergency is unknown until all updates are collected.
        return true;
    }

    void onAlgoKernelStart(Ptr<GraphTileType>& graph) const {
        // For those who do not have in-edges, gather() will never be invoked.
        // Directly set the rank to the teleport value, and never change.
        for (auto vertexIter = graph->vertexIter(); vertexIter != graph->vertexIterEnd(); ++vertexIter) {
            auto& v = vertexIter->second;
            if (v->inDeg() == 0) {
                v->data().rank = 1 - beta_;
            }
        }
    }

protected:
    PageRankEdgeCentricAlgoKernel(const string& name, const double beta, const double tolerance)
        : GraphGASLite::EdgeCentricAlgoKernel<GraphTileType>(name),
          beta_(beta), tolerance_(tolerance)
    {
        // Nothing else to do.
    }

private:
    const double beta_;
    const double tolerance_;
};

#endif // ALGO_KERNELS_EDGE_CENTRIC_PAGERANK_PAGERANK_H_

