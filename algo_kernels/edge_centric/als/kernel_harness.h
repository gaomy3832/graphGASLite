#ifndef KERNEL_HARNESS_H_
#define KERNEL_HARNESS_H_

#include "harness.h"
#include "als.h"

#define R 5

typedef GraphGASLite::GraphTile<GraphGASLite::ALSData<R>, GraphGASLite::ALSUpdate<R>> Graph;
typedef GraphGASLite::ALSEdgeCentricAlgoKernel<Graph> Kernel;

const char appName[] = "als";

class AppArgs : public GenericArgs<double, double> {
public:
    AppArgs() : GenericArgs<double, double>() {
        std::get<0>(argTuple_) = 0.05;
        std::get<1>(argTuple_) = 1e-2;
    };

    const char* name() const { return "<lambda> [ <tolerance> ]"; }
};

/*
 * Assume input graph format as follow:
 * all users have vid less than 10M; other vertices are movies.
 */
#define VERTEX_ARGS ([](const GraphGASLite::VertexIdx& vid){ return vid < 10000000 ? \
        GraphGASLite::Role::USER : GraphGASLite::Role::MOVIE; })

#define VDATA(vd) std::accumulate(vd.ALS().features_.begin(), vd.ALS().features_.end(), string(""),\
        [](const string str, const double a){ return str + " " + std::to_string(a); })

#endif // KERNEL_HARNESS_H_

