#ifndef KERNEL_HARNESS_H_
#define KERNEL_HARNESS_H_

#include "harness.h"
#include "pagerank.h"

typedef GraphGASLite::GraphTile<PageRankData, PageRankUpdate> Graph;
typedef PageRankEdgeCentricAlgoKernel<Graph> Kernel;

const char appName[] = "pagerank";

class AppArgs : public GenericArgs<double, double> {
public:
    AppArgs() : GenericArgs<double, double>() {
        std::get<0>(argTuple_) = betaDefault;
        std::get<1>(argTuple_) = toleranceDefault;
    };

    const ArgInfo* argInfoList() const {
        static const ArgInfo list[] = {
            {"", "[beta]", "Damping factor (default " + std::to_string(betaDefault) + "). Should be between 0 and 1."},
            {"", "[tolerance]", "Error tolerance (default " + std::to_string(toleranceDefault) + ")."},
        };
        return list;
    }

    bool isValid() const {
        auto beta = arg<0>();
        return beta > 0 && beta <= 1;
    }

private:
    static constexpr double betaDefault = 0.85;
    static constexpr double toleranceDefault = 1e-4;
};

#define VDATA(vd) vd.rank

#endif // KERNEL_HARNESS_H_

