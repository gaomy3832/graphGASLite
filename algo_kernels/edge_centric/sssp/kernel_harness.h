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
        std::get<0>(argTuple_) = srcDefault;
    };

    const ArgInfo* argInfoList() const {
        static const ArgInfo list[] = {
            {"", "[src]", "Source vertex index (default " + std::to_string(srcDefault) + ")."},
        };
        return list;
    }

private:
    static constexpr uint64_t srcDefault = 0;
};

#define VDATA(vd) \
    std::to_string(vd.SSSP().distance_) + \
    "\tthrough " + \
    (vd.SSSP().predecessor_ == static_cast<uint64_t>(-1) ? "nan" : std::to_string(vd.SSSP().predecessor_))

#endif // KERNEL_HARNESS_H_

