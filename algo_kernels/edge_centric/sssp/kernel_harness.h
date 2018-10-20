#ifndef KERNEL_HARNESS_H_
#define KERNEL_HARNESS_H_

#include "harness.h"
#include "sssp.h"

typedef GraphGASLite::GraphTile<SSSPData<>, SSSPUpdate<>> Graph;
typedef SSSPEdgeCentricAlgoKernel<Graph> Kernel;

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
    std::to_string(vd.distance) + \
    "\t<- " + \
    (vd.predecessor == INV_VID ? "none" : std::to_string(vd.predecessor))

#endif // KERNEL_HARNESS_H_

