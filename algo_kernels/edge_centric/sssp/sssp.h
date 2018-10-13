#ifndef ALGO_KERNELS_EDGE_CENTRIC_SSSP_SSSP_H_
#define ALGO_KERNELS_EDGE_CENTRIC_SSSP_SSSP_H_

#include "graph.h"
#include "algo_kernel.h"

#define INF(type) std::numeric_limits<type>::max()
#define INV_VID -1uL

/*
 * Graph types definitions.
 */
template<typename EdgeWeightType = uint32_t>
struct SSSPData {
    EdgeWeightType distance;
    GraphGASLite::VertexIdx predecessor;
    GraphGASLite::IterCount activeIter;

    SSSPData(const GraphGASLite::VertexIdx&)
        : distance(INF(EdgeWeightType)), predecessor(INV_VID), activeIter(-1)
    {
        // Nothing else to do.
    }
};

template<typename EdgeWeightType = uint32_t>
struct SSSPUpdate {
    EdgeWeightType distance;
    GraphGASLite::VertexIdx predecessor;

    SSSPUpdate(const EdgeWeightType distance_ = INF(EdgeWeightType), const GraphGASLite::VertexIdx& predecessor_ = INV_VID)
        : distance(distance_), predecessor(predecessor_)
    {
        // Nothing else to do.
    }

    SSSPUpdate& operator+=(const SSSPUpdate& update) {
        // Min of distance.
        if (distance > update.distance) {
            distance = update.distance;
            predecessor = update.predecessor;
        }
        return *this;
    }
};


/*
 * Algorithm kernel definition.
 */
template<typename GraphTileType>
class SSSPEdgeCentricAlgoKernel : public GraphGASLite::EdgeCentricAlgoKernel<GraphTileType> {
public:
    static Ptr<SSSPEdgeCentricAlgoKernel> instanceNew(const string& name,
            const GraphGASLite::VertexIdx& src) {
        return Ptr<SSSPEdgeCentricAlgoKernel>(new SSSPEdgeCentricAlgoKernel(name, src));
    }

protected:
    typedef typename GraphTileType::UpdateType UpdateType;
    typedef typename GraphTileType::VertexType VertexType;
    typedef typename GraphTileType::EdgeType::WeightType EdgeWeightType;

    std::pair<UpdateType, bool> scatter(const GraphGASLite::IterCount& iter, Ptr<VertexType>& src, EdgeWeightType& weight) const {
        auto& data = src->data();
        if (data.activeIter == iter) {
            return std::make_pair(SSSPUpdate<EdgeWeightType>(data.distance + weight, src->vid()), true);
        } else {
            return std::make_pair(SSSPUpdate<EdgeWeightType>(), false);
        }
    }

    bool gather(const GraphGASLite::IterCount& iter, Ptr<VertexType>& dst, const UpdateType& update) const {
        auto& data = dst->data();
        if (data.distance > update.distance) {
            data.distance = update.distance;
            data.predecessor = update.predecessor;
            data.activeIter = iter + 1;
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
        vsrc->data().distance = 0;
        vsrc->data().activeIter = 0;
    }

protected:
    SSSPEdgeCentricAlgoKernel(const string& name, const GraphGASLite::VertexIdx& src)
        : GraphGASLite::EdgeCentricAlgoKernel<GraphTileType>(name),
          src_(src)
    {
        // Nothing else to do.
    }

private:
    const GraphGASLite::VertexIdx src_;
};

#endif // ALGO_KERNELS_EDGE_CENTRIC_SSSP_SSSP_H_

