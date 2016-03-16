#ifndef ALGO_KERNEL_H_
#define ALGO_KERNEL_H_

#include <chrono>
#include <limits>
#include "comm_sync.h"
#include "graph.h"

namespace GraphGASLite {

enum class AlgoKernelTag {
    Invalid,
    EdgeCentric,
    VertexCentric,
};

static inline string algoKernelTagName(const AlgoKernelTag& tag) {
    switch(tag) {
        case AlgoKernelTag::EdgeCentric: return "edge-centric";
        case AlgoKernelTag::VertexCentric: return "vertex-centric";
        case AlgoKernelTag::Invalid:
        default: return "invalid";
    }
}

class IterCountRepType;
typedef CountType<uint64_t, IterCountRepType> IterCount;

static constexpr auto INF_ITER_COUNT = std::numeric_limits<typename IterCount::Type>::max();

template<typename GraphTileType>
class BaseAlgoKernel {
protected:
    typedef CommSync<VertexIdx, typename GraphTileType::UpdateType> CommSyncType;

public:
    /**
     * Algorithm kernel name.
     */
    string name() const { return name_; }

    /**
     * Algorithm kernel tag.
     */
    virtual AlgoKernelTag tag() const {
        return AlgoKernelTag::Invalid;
    }

    /**
     * If print progress.
     */
    bool verbose() const { return verbose_; }
    void verboseIs(const bool verbose) {
        verbose_ = verbose;
    }

    /**
     * Maximum gather-scatter iterations to run.
     */
    IterCount maxIters() const { return maxIters_; }
    void maxItersIs(const IterCount& maxIters) {
        maxIters_ = maxIters;
    }

    /**
     * The number of partitions in each tile.
     *
     * In order to increase locality during gather phase, etc..
     */
    uint32_t numParts() const { return numParts_; }
    void numPartsIs(const uint32_t numParts) {
        numParts_ = numParts;
    }

    /**
     * Call the algorithm kernel. Run iterations.
     *
     * The algorithm kernel is defined as a functor class.
     *
     * @param graph     Graph tile on which this kernel works.
     * @param cs        Utility for comm & sync.
     */
    void operator()(Ptr<GraphTileType>& graph, CommSyncType& cs) const {
        // If need to print progress, i.e., verbose kernel and primary (index 0) tile.
        auto printProgress = verbose() && (graph->tid() == 0);

        // Start barrier, ensure all preparation is done in all threads.
        cs.barrier(graph->tid());

        onAlgoKernelStart(graph);

        IterCount iter(0);
        bool allConverged = false;
        while (!allConverged && iter < maxIters()) {
            bool converged = onIteration(graph, cs, iter);
            onIterationEnd(graph, iter);
            if (printProgress) info("->%lu", iter.cnt());

            // Check if all tiles have converged.
            allConverged = cs.barrierAND(graph->tid(), converged);
            iter++;
        }
        if (printProgress) info("Completed in %lu iterations", iter.cnt());

        onAlgoKernelEnd(graph);
    }

protected:
    /**
     * Operations on end of each iteration.
     */
    virtual void onIterationEnd(Ptr<GraphTileType>& graph, const IterCount& iter) const { }

    /**
     * Operations on start of the algorithm kernel.
     */
    virtual void onAlgoKernelStart(Ptr<GraphTileType>& graph) const { }

    /**
     * Operations on end of the algorithm kernel.
     */
    virtual void onAlgoKernelEnd(Ptr<GraphTileType>& graph) const { }

protected:
    /**
     * Iteration.
     *
     * @param graph     Graph tile on which this kernel works.
     * @param cs        Utility for comm & sync.
     * @param iter      Current iteration count.
     *
     * @return          If converged in this tile.
     */
    virtual bool onIteration(Ptr<GraphTileType>& graph, CommSyncType& cs, const IterCount& iter) const = 0;

protected:
    string name_;
    bool verbose_;
    IterCount maxIters_;
    uint32_t numParts_;

protected:
    BaseAlgoKernel(const string& name)
        : name_(name), verbose_(false), maxIters_(INF_ITER_COUNT), numParts_(1)
    {
        // Nothing else to do.
    }

    BaseAlgoKernel(const BaseAlgoKernel&) = delete;
    BaseAlgoKernel& operator=(const BaseAlgoKernel&) = delete;
    BaseAlgoKernel(BaseAlgoKernel&&) = delete;
    BaseAlgoKernel& operator=(BaseAlgoKernel&&) = delete;
    bool operator==(const BaseAlgoKernel&) const = delete;

};

template<typename GraphTileType>
class EdgeCentricAlgoKernel : public BaseAlgoKernel<GraphTileType> {
public:
    typedef typename GraphTileType::VertexType VertexType;
    typedef typename GraphTileType::EdgeType::WeightType EdgeWeightType;
    typedef typename GraphTileType::UpdateType UpdateType;

public:
    AlgoKernelTag tag() const final {
        return AlgoKernelTag::EdgeCentric;
    }

protected:
    /**
     * Edge-centric scatter function.
     *
     * @param iter      Current iteration count.
     * @param src       Source vertex.
     * @param weight    Weight of the edge.
     *
     * @return          A pair consisting of the output update data, and a bool
     *                  denoting whether the update is valid.
     */
    virtual std::pair<UpdateType, bool>
    scatter(const IterCount& iter, Ptr<VertexType>& src, EdgeWeightType& weight) const = 0;

    /**
     * Edge-centric gather function.
     *
     * @param iter      Current iteration count.
     * @param dst       Destination vertex.
     * @param update    Input update data.
     *
     * @return          Whether this vertex is converged.
     */
    virtual bool
    gather(const IterCount& iter, Ptr<VertexType>& dst, const UpdateType& update) const = 0;

protected:
    using typename BaseAlgoKernel<GraphTileType>::CommSyncType;
    bool onIteration(Ptr<GraphTileType>& graph, CommSyncType& cs, const IterCount& iter) const final;

protected:
    EdgeCentricAlgoKernel(const string& name)
        : BaseAlgoKernel<GraphTileType>(name)
    {
        // Nothing else to do.
    }
};

template<typename GraphTileType>
class VertexCentricAlgoKernel : public BaseAlgoKernel<GraphTileType> {
public:
    typedef typename GraphTileType::VertexType VertexType;
    typedef typename GraphTileType::EdgeType::WeightType EdgeWeightType;
    typedef typename GraphTileType::UpdateType UpdateType;

public:
    AlgoKernelTag tag() const final {
        return AlgoKernelTag::VertexCentric;
    }

protected:
    /**
     * Vertex-centric gather function.
     *
     * @param iter      Current iteration count.
     * @param src       Source vertex.
     * @param weight    Weight of the edge.
     *
     * @return          Output update data.
     */
    virtual UpdateType
    gather(const IterCount& iter, Ptr<VertexType>& src, EdgeWeightType& weight) const = 0;

    /**
     * Vertex-centric apply function.
     *
     * @param iter      Current iteration count.
     * @param v         Vertex.
     * @param accUpdate Accumulated update data.
     */
    virtual void
    apply(const IterCount& iter, Ptr<VertexType>& v, const UpdateType& accUpdate) const = 0;

    /**
     * Vertex-centric scatter function.
     *
     * @param iter      Current iteration count.
     * @param src       Source vertex.
     *
     * @return          Whether to activate the destination vertex for gathering
     *                  in the next iteration.
     */
    virtual bool
    scatter(const IterCount& iter, Ptr<VertexType>& src) const = 0;

protected:
    using typename BaseAlgoKernel<GraphTileType>::CommSyncType;
    bool onIteration(Ptr<GraphTileType>& graph, CommSyncType& cs, const IterCount& iter) const final;

protected:
    VertexCentricAlgoKernel(const string& name)
        : BaseAlgoKernel<GraphTileType>(name)
    {
        // Nothing else to do.
    }
};



template<typename GraphTileType>
bool EdgeCentricAlgoKernel<GraphTileType>::
onIteration(Ptr<GraphTileType>& graph, CommSyncType& cs, const IterCount& iter) const {

    const auto tid = graph->tid();

    // As there is no explicit barrier in each iteration, the earliest time
    // when the producer can know it is safe to reset comm. utility, is at the
    // beginning of the next iteration after the barrier b/w iterations.
    cs.keyValProdDelAll(tid);

    // Scatter.
    Ptr<VertexType> cachedSrc = nullptr;
    VertexIdx cachedSrcId(-1);
    for (auto edgeIter = graph->edgeIter(); edgeIter != graph->edgeIterEnd(); ++edgeIter) {
        const auto srcId = edgeIter->srcId();
        const auto dstId = edgeIter->dstId();
        // Return reference to allow update to weight.
        auto& weight = edgeIter->weight();

        // Cache the src to save vertex map search because edges are sorted by the src vertex.
        if (srcId != cachedSrcId) {
            cachedSrc = graph->vertex(srcId);
            cachedSrcId = srcId;
        }

        // Scatter.
        auto ret = scatter(iter, cachedSrc, weight);
        if (ret.second) {
            const auto& update = ret.first;
            if (graph->vertex(dstId) != nullptr) {
                // Local destination.
                cs.keyValNew(tid, tid, dstId, update);
            } else {
                // Remote destination, use mirror vertex.
                auto mv = graph->mirrorVertex(dstId);
                mv->updateNew(update);
            }
        }
    }

    // Send data.
    for (auto mvIter = graph->mirrorVertexIter(); mvIter != graph->mirrorVertexIterEnd(); ++mvIter) {
        auto mv = mvIter->second;
        const auto dstTileId = mv->masterTileId();
        const auto dstId = mv->vid();
        const auto& accUpdate = mv->accUpdate();
        cs.keyValNew(tid, dstTileId, dstId, accUpdate);
        // Clear updates in mirror vertex.
        mv->updateDelAll();
    }

    for (uint32_t idx = 0; idx < cs.threadCount(); idx++) {
        cs.endTagNew(tid, idx);
    }

    // Receive data and gather.
    bool converged = true;
    auto hf = std::hash<VertexIdx::Type>();
    auto dstIdHash = [&hf](const VertexIdx& k) {
        return hf(k);
    };
    while (true) {
        auto recvData = cs.keyValPartitions(tid, this->numParts(), dstIdHash);
        const auto& updatePartitions = recvData.first;
        auto recvStatus = recvData.second;

        if (recvStatus == CommSyncType::RECV_NONE) {
            // Sleep shortly to wait for data.
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            continue;
        }

        // For each subpartition ...
        for (const auto& prtn : updatePartitions) {
            // For each update ...
            for (const auto& u : prtn) {
                const auto dstId = u.key();
                const auto& update = u.val();

                // Gather.
                auto dst = graph->vertex(dstId);
                converged &= gather(iter, dst, update);
            }
        }

        // Finish receiving.
        if (recvStatus == CommSyncType::RECV_FINISHED) break;
    }

    cs.keyValConsDelAll(tid);

    return converged;
}

} // namespace GraphGASLite

#endif // ALGO_KERNEL_H_
