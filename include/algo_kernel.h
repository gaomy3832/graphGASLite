#ifndef ALGO_KERNEL_H_
#define ALGO_KERNEL_H_

#include <limits>
#include "graph.h"

template<typename GraphTileType>
class BaseAlgoKernel {
public:
    class IterCountRepType;
    typedef CountType<uint64_t, IterCountRepType> IterCount;

public:
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
     */
    virtual void operator()(Ptr<GraphTileType>& graph) = 0;

protected:
    IterCount maxIters_;
    uint32_t numParts_;

protected:
    BaseAlgoKernel(
        const IterCount& maxIters = std::numeric_limits<typename IterCount::Type>::max(),
        const uint32_t numParts = 1
    )
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

#endif // ALGO_KERNEL_H_
