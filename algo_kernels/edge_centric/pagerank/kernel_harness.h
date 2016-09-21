#ifndef KERNEL_HARNESS_H_
#define KERNEL_HARNESS_H_

#include "harness.h"
#include "pagerank.h"

typedef GraphGASLite::GraphTile<GraphGASLite::PageRankData, GraphGASLite::PageRankUpdate> Graph;
typedef GraphGASLite::PageRankEdgeCentricAlgoKernel<Graph> Kernel;

const char appName[] = "pagerank";

class AppArgs : public GenericArgs<double, double> {
public:
    AppArgs() : GenericArgs<double, double>() {
        std::get<0>(argTuple_) = 0.85;
        std::get<1>(argTuple_) = 1e-4;
    };

    const char* name() const { return "<beta> [ <tolerance> ]"; }
};

#define VDATA(vd) vd.PAGERANK().rank_

#endif // KERNEL_HARNESS_H_

