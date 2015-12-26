#ifndef ALGO_KERNEL_H_
#define ALGO_KERNEL_H_

#include <limits>
#include "graph.h"

template<typename GraphTileType>
class BaseAlgoKernel {
public:
    /**
     * Maximum gather-scatter iterations to run.
     */
    uint64_t maxIters() const { return maxIters_; }
    void maxItersIs(const uint64_t maxIters) {
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
    uint64_t maxIters_;
    uint32_t numParts_;

protected:
    BaseAlgoKernel(const uint64_t maxIters = std::numeric_limits<uint64_t>::max(), const uint32_t numParts = 1)
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
