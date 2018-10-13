#ifndef KERNEL_HARNESS_H_
#define KERNEL_HARNESS_H_

#include "harness.h"
#include "als.h"

#define R 5

typedef GraphGASLite::GraphTile<ALSData<R>, ALSUpdate<R>> Graph;
typedef ALSEdgeCentricAlgoKernel<Graph> Kernel;

const char appName[] = "als";

class AppArgs : public GenericArgs<double, double> {
public:
    AppArgs() : GenericArgs<double, double>() {
        std::get<0>(argTuple_) = lambdaDefault;
        std::get<1>(argTuple_) = toleranceDefault;
    };

    const ArgInfo* argInfoList() const {
        static const ArgInfo list[] = {
            {"", "[lambda]", "Regulation coefficient (default " + std::to_string(lambdaDefault) + ")."},
            {"", "[tolerance]", "Error tolerance (default " + std::to_string(toleranceDefault) + ")."},
        };
        return list;
    }

private:
    static constexpr double lambdaDefault = 0.05;
    static constexpr double toleranceDefault = 1e-2;
};

#define VDATA(vd) std::accumulate(vd.features.begin(), vd.features.end(), string(""),\
        [](const string str, const double a){ return str + " " + std::to_string(a); })

#endif // KERNEL_HARNESS_H_

