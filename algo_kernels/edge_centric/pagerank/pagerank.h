#ifndef ALGO_KERNELS_EDGE_CENTRIC_PAGERANK_PAGERANK_H_
#define ALGO_KERNELS_EDGE_CENTRIC_PAGERANK_PAGERANK_H_

#include "graph.h"
#include "algo_kernel.h"

namespace GraphGASLite {

/*
 * Graph types definition.
 *
 * Define PAGERANK() in graph data and update to get PageRankData and PageRankUpdate.
 */
struct PageRankData {
    DegreeCount collected_;
    double sum_;
    double rank_;

    PageRankData(const VertexIdx&)
        : collected_(0), sum_(0), rank_(0)
    {
        // Nothing else to do.
    }

    inline PageRankData& PAGERANK() { return *this; }
    inline const PageRankData& PAGERANK() const { return *this; }
};

struct PageRankUpdate {
    double contribute_;
    DegreeCount count_;

    PageRankUpdate(const double contribute = 0, const DegreeCount& count = 0)
        : contribute_(contribute), count_(count)
    {
        // Nothing else to do.
    }

    PageRankUpdate& operator+=(const PageRankUpdate& update) {
        this->contribute_ += update.contribute_;
        this->count_ += update.count_;
        return *this;
    }

    inline PageRankUpdate& PAGERANK() { return *this; }
    inline const PageRankUpdate& PAGERANK() const { return *this; }
};


/*
 * Algorithm kernel definition.
 */
template<typename GraphTileType>
class PageRankEdgeCentricAlgoKernel : public EdgeCentricAlgoKernel<GraphTileType> {
public:
    static Ptr<PageRankEdgeCentricAlgoKernel> instanceNew(const string& name,
            const double beta, const double tolerance) {
        return Ptr<PageRankEdgeCentricAlgoKernel>(new PageRankEdgeCentricAlgoKernel(name, beta, tolerance));
    }

protected:
    typedef typename GraphTileType::UpdateType UpdateType;
    typedef typename GraphTileType::VertexType VertexType;
    typedef typename GraphTileType::EdgeType::WeightType EdgeWeightType;

    std::pair<UpdateType, bool> scatter(const IterCount&, Ptr<VertexType>& src, EdgeWeightType&) const {
        auto& dpr = src->data().PAGERANK();
        auto contribute = dpr.rank_ / src->outDeg();
        std::pair<UpdateType, bool> ret;
        ret.first.PAGERANK() = PageRankUpdate(contribute, 1);
        ret.second = true;
        return ret;
    }

    bool gather(const IterCount&, Ptr<VertexType>& dst, const UpdateType& update) const {
        auto& dpr = dst->data().PAGERANK();
        auto& upr = update.PAGERANK();
        dpr.sum_ += upr.contribute_;
        dpr.collected_ += upr.count_;
        if (dpr.collected_ == dst->inDeg()) {
            double newRank = beta_ * dpr.sum_ + (1 - beta_);
            bool converge = (std::abs(newRank - dpr.rank_) <= tolerance_);
            dpr.rank_ = newRank;
            dpr.sum_ = 0;
            dpr.collected_ = 0;
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
                v->data().PAGERANK().rank_ = 1 - beta_;
            }
        }
    }

protected:
    PageRankEdgeCentricAlgoKernel(const string& name, const double beta, const double tolerance)
        : EdgeCentricAlgoKernel<GraphTileType>(name),
          beta_(beta), tolerance_(tolerance)
    {
        // Nothing else to do.
    }

private:
    const double beta_;
    const double tolerance_;
};

} // namespace GraphGASLite

#endif // ALGO_KERNELS_EDGE_CENTRIC_PAGERANK_PAGERANK_H_

