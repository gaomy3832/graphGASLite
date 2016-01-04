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
     * Instantiate an instance.
     */
    static Ptr<BaseAlgoKernel> instanceNew(
        const IterCount& maxIters = INF_ITER_COUNT,
        const uint32_t numParts = 1
    ) {
        return Ptr<BaseAlgoKernel>(new BaseAlgoKernel(maxIters, numParts));
    }

    /**
     * Algorithm kernel tag.
     */
    virtual AlgoKernelTag tag() const {
        return AlgoKernelTag::Invalid;
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
    void operator()(Ptr<GraphTileType>& graph, CommSync& cs) {


    }

protected:
    IterCount maxIters_;
    uint32_t numParts_;

protected:
    BaseAlgoKernel(const IterCount& maxIters, const uint32_t numParts)
        : maxIters_(maxIters), numParts_(numParts)
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
    using BaseAlgoKernel<GraphTileType>::INF_ITER_COUNT;

public:
    static Ptr<EdgeCentricAlgoKernel> instanceNew(
        const IterCount& maxIters = INF_ITER_COUNT,
        const uint32_t numParts = 1
    ) {
        return Ptr<EdgeCentricAlgoKernel>(new EdgeCentricAlgoKernel(maxIters, numParts));
    }

    AlgoKernelTag tag() const {
        return AlgoKernelTag::EdgeCentric;
    }

    /**
     * Clone the algorithm kernel.
     */
    Ptr<EdgeCentricAlgoKernel> clone() const {
        return EdgeCentricAlgoKernel::instanceNew(this->maxIters_, this->numParts_);
    }

protected:
    EdgeCentricAlgoKernel(const IterCount& maxIters, const uint32_t numParts)
        : BaseAlgoKernel<GraphTileType>(maxIters, numParts)
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
    using BaseAlgoKernel<GraphTileType>::INF_ITER_COUNT;

public:
    static Ptr<VertexCentricAlgoKernel> instanceNew(
        const IterCount& maxIters = INF_ITER_COUNT,
        const uint32_t numParts = 1
    ) {
        return Ptr<VertexCentricAlgoKernel>(new VertexCentricAlgoKernel(maxIters, numParts));
    }

    AlgoKernelTag tag() const {
        return AlgoKernelTag::VertexCentric;
    }

    /**
     * Clone the algorithm kernel.
     */
    Ptr<VertexCentricAlgoKernel> clone() const {
        return VertexCentricAlgoKernel::instanceNew(this->maxIters_, this->numParts_);
    }

protected:
    VertexCentricAlgoKernel(const IterCount& maxIters, const uint32_t numParts)
        : BaseAlgoKernel<GraphTileType>(maxIters, numParts)
    {
        // Nothing else to do.
    }
};

#endif // ALGO_KERNEL_H_
