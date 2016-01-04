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
    string name_;
    IterCount maxIters_;
    uint32_t numParts_;

protected:
    BaseAlgoKernel(const string& name, const IterCount& maxIters, const uint32_t numParts)
        : name_(name), maxIters_(maxIters), numParts_(numParts)
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
    AlgoKernelTag tag() const final {
        return AlgoKernelTag::EdgeCentric;
    }

    /**
     * Clone the algorithm kernel.
     */
    Ptr<EdgeCentricAlgoKernel> clone(const string& name) const {
        return Ptr<EdgeCentricAlgoKernel>(new EdgeCentricAlgoKernel(name, this->maxIters_, this->numParts_));
    }

protected:
    EdgeCentricAlgoKernel(
        const string& name,
        const IterCount& maxIters = INF_ITER_COUNT,
        const uint32_t numParts = 1
    )
        : BaseAlgoKernel<GraphTileType>(name, maxIters, numParts)
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
    AlgoKernelTag tag() const final {
        return AlgoKernelTag::VertexCentric;
    }

    /**
     * Clone the algorithm kernel.
     */
    Ptr<VertexCentricAlgoKernel> clone(const string& name) const {
        return Ptr<VertexCentricAlgoKernel>(new VertexCentricAlgoKernel(name, this->maxIters_, this->numParts_));
    }

protected:
    VertexCentricAlgoKernel(
        const string& name,
        const IterCount& maxIters = INF_ITER_COUNT,
        const uint32_t numParts = 1
    )
        : BaseAlgoKernel<GraphTileType>(name, maxIters, numParts)
    {
        // Nothing else to do.
    }
};

#endif // ALGO_KERNEL_H_
