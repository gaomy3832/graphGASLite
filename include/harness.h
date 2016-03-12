#ifndef HARNESS_H_
#define HARNESS_H_

#include <functional>
#include <iostream>
#include <iomanip>
#include <sstream>
#include <tuple>
#include <unistd.h>


void usage(const char* appName, const char* appArgsName) {
    std::cerr <<
        "Usage: " << appName << " [options] "
        "<input edge list file> [ <partition file> [ <output file> [ " <<
        appArgsName <<
        " ] ] ]" << std::endl;
}


/**********************************************************
 *
 * Algorithm kernel common arguments.
 *
 **********************************************************/

constexpr uint64_t maxItersDefault = 1000;
constexpr uint32_t numPartsDefault = 16;

void algoKernelComArgHelp() {
    constexpr int w = 15;
    std::cerr << "Options:" << std::endl
        << '\t' << "-t " << std::left << std::setw(w) << "<threads>"
        << "Number of threads (required)." << std::endl
        << '\t' << "-g " << std::left << std::setw(w) << "<gtiles>"
        << "Number of graph tiles (required). Should be multiplier of threads." << std::endl
        << '\t' << "-m " << std::left << std::setw(w) << "<maxiter>"
        << "Maximum iteration number (default " << maxItersDefault << ")." << std::endl
        << '\t' << "-p " << std::left << std::setw(w) << "<numParts>"
        << "Number of partitions per thread (default " << numPartsDefault << ")." << std::endl
        << '\t' << "-h " << std::left << std::setw(w) << ""
        << "Print this help message." << std::endl;
}

/**
 * Parse command line options to common algo kernel parameters.
 *
 * @return              Number of args parsed. Return -1 on failure.
 */
int algoKernelComArg(const int argc, char* const* argv,
        size_t& threadCount, size_t& graphTileCount,
        uint64_t& maxIters, uint32_t& numParts) {

    threadCount = 0;
    graphTileCount = 0;
    maxIters = maxItersDefault;
    numParts = numPartsDefault;

    int ch;
    opterr = 0; // Reset potential previous errors.
    while ((ch = getopt(argc, argv, "t:g:m:p:h")) != -1) {
        switch (ch) {
            case 't':
                std::stringstream(optarg) >> threadCount;
                break;
            case 'g':
                std::stringstream(optarg) >> graphTileCount;
                break;
            case 'm':
                std::stringstream(optarg) >> maxIters;
                break;
            case 'p':
                std::stringstream(optarg) >> numParts;
                break;
            case 'h':
            default:
                algoKernelComArgHelp();
                return -1;
        }
    }

    if (threadCount == 0 || graphTileCount == 0) {
        std::cerr << "Must specify number of threads and number of graph tiles." << std::endl;
        algoKernelComArgHelp();
        return -1;
    }
    if (graphTileCount % threadCount != 0) {
        std::cerr << "Number of threads must be a divisor of number of graph tiles." << std::endl;
        algoKernelComArgHelp();
        return -1;
    }

    return optind;
}


/**********************************************************
 *
 * Application specific arguments.
 *
 **********************************************************/

/**
 * Generic arguments.
 */
template<typename... ArgTypes>
class GenericArgs {
public:
    static constexpr size_t argCount = sizeof...(ArgTypes);

    const std::tuple<ArgTypes...>& argTuple() const { return argTuple_; }

    template<size_t N>
    const typename std::tuple_element<N, std::tuple<ArgTypes...>>::type arg() const {
        return std::get<N>(argTuple_);
    }

    template<typename R>
    R functionIs(std::function<R(ArgTypes...)>& func) {
        return dispatch(func, typename GenSeq<sizeof...(ArgTypes)>::Type());
    }

    virtual const char* name() const = 0;

    virtual void argIs(int argc, char* argv[]) = 0;

protected:
    std::tuple<ArgTypes...> argTuple_;

protected:
    /**
     * Unpack tuple to variadic function arguments for function dispatch.
     *
     * http://stackoverflow.com/questions/7858817/unpacking-a-tuple-to-call-a-matching-function-pointer
     */
    template<int ...>
    struct Seq { };

    template<int N, int... S>
    struct GenSeq : GenSeq<N-1, N-1, S...> { };

    template<int... S>
    struct GenSeq<0, S...> { typedef Seq<S...> Type; };

    template<typename R, int... S>
    R dispatch(std::function<R(ArgTypes...)>& func, Seq<S...>) {
        return func(std::get<S>(argTuple_)...);
    }
};


#endif // HARNESS_H_

