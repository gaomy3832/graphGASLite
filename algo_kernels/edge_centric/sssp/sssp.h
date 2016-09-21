#ifndef ALGO_KERNELS_EDGE_CENTRIC_SSSP_SSSP_H_
#define ALGO_KERNELS_EDGE_CENTRIC_SSSP_SSSP_H_

#include "graph.h"
#include "algo_kernel.h"

namespace GraphGASLite {

/*
 * Graph types definition.
 *
 * Define SSSP() in graph data and update to get SSSPData and SSSPUpdate.
 */
template<typename EdgeWeightType = uint32_t>
struct SSSPData {
    EdgeWeightType distance_;
    VertexIdx predecessor_;
    IterCount activeIter_;

    SSSPData(const VertexIdx&)
        : distance_(std::numeric_limits<EdgeWeightType>::max()), predecessor_(-1), activeIter_(-1)
    {
        // Nothing else to do.
    }

    inline SSSPData& SSSP() { return *this; }
    inline const SSSPData& SSSP() const { return *this; }
};

template<typename EdgeWeightType = uint32_t>
struct SSSPUpdate {
    EdgeWeightType distance_;
    VertexIdx predecessor_;

    SSSPUpdate(const EdgeWeightType distance = std::numeric_limits<EdgeWeightType>::max(), const VertexIdx& predecessor = -1)
        : distance_(distance), predecessor_(predecessor)
    {
        // Nothing else to do.
    }

    SSSPUpdate& operator+=(const SSSPUpdate& update) {
        // Min of distance.
        if (this->distance_ > update.distance_) {
            this->distance_ = update.distance_;
            this->predecessor_ = update.predecessor_;
        }
        return *this;
    }

    inline SSSPUpdate& SSSP() { return *this; }
    inline const SSSPUpdate& SSSP() const { return *this; }
};


/*
 * Algorithm kernel definition.
 */
template<typename GraphTileType>
class SSSPEdgeCentricAlgoKernel : public EdgeCentricAlgoKernel<GraphTileType> {
public:
    static Ptr<SSSPEdgeCentricAlgoKernel> instanceNew(const string& name,
            const VertexIdx& src) {
        return Ptr<SSSPEdgeCentricAlgoKernel>(new SSSPEdgeCentricAlgoKernel(name, src));
    }

protected:
    typedef typename GraphTileType::UpdateType UpdateType;
    typedef typename GraphTileType::VertexType VertexType;
    typedef typename GraphTileType::EdgeType::WeightType EdgeWeightType;

    std::pair<UpdateType, bool> scatter(const IterCount& iter, Ptr<VertexType>& src, EdgeWeightType& weight) const {
        auto& d = src->data().SSSP();
        if (d.activeIter_ == iter) {
            return std::make_pair(SSSPUpdate<EdgeWeightType>(d.distance_ + weight, src->vid()), true);
        } else {
            return std::make_pair(SSSPUpdate<EdgeWeightType>(), false);
        }
    }

    bool gather(const IterCount& iter, Ptr<VertexType>& dst, const UpdateType& update) const {
        auto& d = dst->data().SSSP();
        auto& u = update.SSSP();
        if (u.distance_ < d.distance_) {
            d.distance_ = u.distance_;
            d.predecessor_ = u.predecessor_;
            d.activeIter_ = iter + 1;
            // Not converged.
            return false;
        }
        return true;
    }

    void onAlgoKernelStart(Ptr<GraphTileType>& graph) const {
        // Set source vertex.
        auto vsrc = graph->vertex(src_);
        // Source vertex does not exist.
        if (vsrc == nullptr) return;
        vsrc->data().SSSP().distance_ = 0;
        vsrc->data().SSSP().activeIter_ = 0;
    }

protected:
    SSSPEdgeCentricAlgoKernel(const string& name, const VertexIdx& src)
        : EdgeCentricAlgoKernel<GraphTileType>(name),
          src_(src)
    {
        // Nothing else to do.
    }

private:
    const VertexIdx src_;
};

} // namespace GraphGASLite

#endif // ALGO_KERNELS_EDGE_CENTRIC_SSSP_SSSP_H_

