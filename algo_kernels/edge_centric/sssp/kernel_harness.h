#ifndef KERNEL_HARNESS_H_
#define KERNEL_HARNESS_H_

#include "harness.h"
#include "sssp.h"

typedef GraphGASLite::GraphTile<GraphGASLite::SSSPData<>, GraphGASLite::SSSPUpdate<>> Graph;
typedef GraphGASLite::SSSPEdgeCentricAlgoKernel<Graph> Kernel;

const char appName[] = "sssp";

class AppArgs : public GenericArgs<uint64_t> {
public:
    AppArgs() : GenericArgs<uint64_t>() {
        std::get<0>(argTuple_) = 0;
    };

    const char* name() const { return "<src vid>"; }
};

#define VDATA(vd) \
    std::to_string(vd.SSSP().distance_) + \
    "\tthrough " + \
    (vd.SSSP().predecessor_ == static_cast<uint64_t>(-1) ? "nan" : std::to_string(vd.SSSP().predecessor_))

#endif // KERNEL_HARNESS_H_

