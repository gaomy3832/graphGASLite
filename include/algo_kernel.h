#ifndef ALGO_KERNEL_H_
#define ALGO_KERNEL_H_

#include <limits>
#include "comm_sync.h"
#include "graph.h"

enum class AlgoKernelTag {
    Invalid,
    EdgeCentric,
    VertexCentric,
};

template<typename GraphTileType>
class BaseAlgoKernel {
public:
    class IterCountRepType;
    typedef CountType<uint64_t, IterCountRepType> IterCount;

    static constexpr auto INF_ITER_COUNT = std::numeric_limits<typename IterCount::Type>::max();

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
    void operator()(Ptr<GraphTileType>& graph, CommSync& cs) const {
        // If need to print progress, i.e., verbose kernel and primary (index 0) tile.
        auto printProgress = verbose() && (graph->tid() == 0);

        // Start barrier, ensure all preparation is done in all threads.
        cs.barrier();

        onAlgoKernelStart(graph);

        IterCount iter(0);
        bool allConverged = false;
        while (!allConverged && iter < maxIters()) {
            bool converged = onIteration(graph, cs, iter);
            onIterationEnd(graph, iter);
            if (printProgress) info("->%lu", iter.cnt());

            // Check if all tiles have converged.
            allConverged = cs.barrierAND(converged);
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
    virtual bool onIteration(Ptr<GraphTileType>& graph, CommSync& cs, const IterCount& iter) const = 0;

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

template<typename GraphTileType, typename UpdateType>
class EdgeCentricAlgoKernel : public BaseAlgoKernel<GraphTileType> {
public:
    typedef typename GraphTileType::VertexType VertexType;
    typedef typename GraphTileType::EdgeType::WeightType EdgeWeightType;
    using typename BaseAlgoKernel<GraphTileType>::IterCount;

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
     * @return          Whether this vertex needs further update (not converged).
     */
    virtual bool
    gather(const IterCount& iter, Ptr<VertexType>& dst, const UpdateType& update) const = 0;

protected:
    bool onIteration(Ptr<GraphTileType>& graph, CommSync& cs, const IterCount& iter) const final;

protected:
    EdgeCentricAlgoKernel(const string& name)
        : BaseAlgoKernel<GraphTileType>(name)
    {
        // Nothing else to do.
    }
};

template<typename GraphTileType, typename UpdateType>
class VertexCentricAlgoKernel : public BaseAlgoKernel<GraphTileType> {
public:
    typedef typename GraphTileType::VertexType VertexType;
    typedef typename GraphTileType::EdgeType::WeightType EdgeWeightType;
    using typename BaseAlgoKernel<GraphTileType>::IterCount;

public:
    AlgoKernelTag tag() const final {
        return AlgoKernelTag::VertexCentric;
    }

protected:
    VertexCentricAlgoKernel(const string& name)
        : BaseAlgoKernel<GraphTileType>(name)
    {
        // Nothing else to do.
    }
};

#endif // ALGO_KERNEL_H_
