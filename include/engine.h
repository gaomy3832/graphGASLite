#ifndef ENGINE_H_
#define ENGINE_H_

#include <algorithm>
#include <unordered_map>
#include <vector>
#include "utils/thread_pool.h"
#include "algo_kernel.h"
#include "comm_sync.h"
#include "graph.h"

namespace GraphGASLite {

template<typename GraphTileType>
class Engine {
public:
    typedef BaseAlgoKernel<GraphTileType> AlgoKernelType;

    typedef std::vector< Ptr<GraphTileType> > GraphTileList;

    typedef std::vector< Ptr<const AlgoKernelType> > AlgoKernelList;
    typedef typename AlgoKernelList::iterator AlgoKernelIter;
    typedef typename AlgoKernelList::const_iterator AlgoKernelConstIter;

public:
    Ptr<GraphTileType> graphTile(const TileIdx& tid) const {
        if (tid >= graphs_.size()) return nullptr;
        return graphs_[tid];
    }
    size_t graphTileCount() const {
        return graphs_.size();
    }
    /**
     * Append a single graph tile \c graphTile.
     *
     * The appended tile must have incremental tile index.
     */
    void graphTileNew(const Ptr<GraphTileType>& graphTile) {
        if (graphTile == nullptr) {
            throw NullPointerException("graphTile");
        }
        // Graph tile index must be incremental.
        if (graphTile->tid() != graphs_.size()) {
            throw InvalidArgumentException("graphTile");
        }
        graphs_.push_back(graphTile);
    }
    /**
     * Copy-assign all graph tiles.
     *
     * Each tile in \c graphs must have incremental tile index.
     */
    void graphTileIs(const GraphTileList& graphs) {
        for (uint32_t idx = 0; idx < graphs.size(); idx++) {
            if (graphs[idx] == nullptr) {
                throw NullPointerException("graphs[" + std::to_string(idx) + "]");
            }
            // Test graph tile list is incremental.
            if (graphs[idx]->tid() != idx) {
                throw InvalidArgumentException("graphs[" + std::to_string(idx) + "]");
            }
        }
        graphs_ = graphs;
    }
    /**
     * Move-assign all graph tiles.
     *
     * Each tile in \c graphs must have incremental tile index.
     */
    void graphTileIs(GraphTileList&& graphs) {
        for (uint32_t idx = 0; idx < graphs.size(); idx++) {
            if (graphs[idx] == nullptr) {
                throw NullPointerException("graphs[" + std::to_string(idx) + "]");
            }
            // Test graph tile list is incremental.
            if (graphs[idx]->tid() != idx) {
                throw InvalidArgumentException("graphs[" + std::to_string(idx) + "]");
            }
        }
        graphs_.swap(graphs);
    }

    size_t algoKernelCount() const {
        return kernels_.size();
    }
    AlgoKernelIter algoKernelIter() {
        return kernels_.begin();
    }
    AlgoKernelIter algoKernelIterEnd() {
        return kernels_.end();
    }
    AlgoKernelConstIter algoKernelConstIter() const {
        return kernels_.cbegin();
    }
    AlgoKernelConstIter algoKernelConstIterEnd() const {
        return kernels_.cend();
    }
    /**
     * Append an algorithm kernel.
     */
    void algoKernelNew(const Ptr<AlgoKernelType>& kernel) {
        if (kernel == nullptr) {
            throw NullPointerException("kernel");
        }
        kernels_.push_back(kernel);
    }
    /**
     * Delete an algorithm kernel.
     */
    AlgoKernelIter algoKernelDel(const AlgoKernelIter& iter) {
        return kernels_.erase(iter);
    }

    /**
     * Run all algorithm kernels in sequence on the graph tiles.
     *
     * The engine is defined as a functor class.
     */
    void operator()() {
        // Number of worker threads.
        // Currently use one thread for each tile.
        auto threadCount = graphTileCount();

        // Utility for communication and synchronization.
        typedef CommSync<VertexIdx, typename GraphTileType::UpdateType> CommSyncType;
        CommSyncType cs(threadCount,
                typename CommSyncType::KeyValue(-1uL, typename GraphTileType::UpdateType()));

        // Construct thread input data.
        struct ThreadData {
            Ptr<GraphTileType> graphTile_;
            AlgoKernelList kernels_;
            CommSyncType* cs_;
        };
        std::vector<ThreadData> threadData;
        for (auto graphTile : graphs_) {
            // Clone the algorithm kernels for each worker.
            AlgoKernelList kernels;
            for (auto k : kernels_) {
                kernels.push_back(k);
            }
            threadData.push_back({graphTile, kernels, &cs});
        }

        // Thread function.
        auto threadRegisterFunc = [](ThreadData& td) {
            td.cs_->threadIdIs(td.graphTile_->tid());
        };
        auto threadFunc = [](ThreadData& td) {
            for (auto& k : td.kernels_) {
                (*k)(td.graphTile_, *td.cs_);
            }
        };

        // Thread pool.
        ThreadPool pool(threadCount);
        for (auto& td : threadData) {
            pool.add_task(std::bind(threadRegisterFunc, td));
        }
        pool.wait_all();
        for (auto& td : threadData) {
            pool.add_task(std::bind(threadFunc, td));
        }
        pool.wait_all();
    }

protected:
    GraphTileList graphs_;
    AlgoKernelList kernels_;

};

} // namespace GraphGASLite

#endif // ENGINE_H_
