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
    void operator()(Ptr<GraphTileType>& graph, CommSync& cs) {


    }

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

    /**
     * Clone the algorithm kernel.
     */
    Ptr<EdgeCentricAlgoKernel> clone(const string& name) const {
        auto c = Ptr<EdgeCentricAlgoKernel>(new EdgeCentricAlgoKernel(name));
        c->verboseIs(this->verbose());
        c->maxItersIs(this->maxIters());
        c->numPartsIs(this->numParts());
        return c;
    }

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

    /**
     * Clone the algorithm kernel.
     */
    Ptr<VertexCentricAlgoKernel> clone(const string& name) const {
        auto c = Ptr<VertexCentricAlgoKernel>(new VertexCentricAlgoKernel(name));
        c->verboseIs(this->verbose());
        c->maxItersIs(this->maxIters());
        c->numPartsIs(this->numParts());
        return c;
    }

protected:
    VertexCentricAlgoKernel(const string& name)
        : BaseAlgoKernel<GraphTileType>(name)
    {
        // Nothing else to do.
    }
};

#endif // ALGO_KERNEL_H_
