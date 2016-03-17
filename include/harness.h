#ifndef HARNESS_H_
#define HARNESS_H_

#include <functional>
#include <iostream>
#include <iomanip>
#include <sstream>
#include <tuple>
#include <unistd.h>
#include "common.h"


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

    friend std::ostream& operator<<(std::ostream& os, const GenericArgs& args) {
        StreamOutputFunc func(os);
        args.foreach(func);
        return os;
    }

    template<typename KernelType>
    Ptr<KernelType> algoKernel(const string& kernelName) {
        return dispatch(KernelType::instanceNew, typename GenSeq<sizeof...(ArgTypes)>::Type(), kernelName);
    }

    virtual const char* name() const = 0;

    virtual void argIs(int argc, char* argv[]) {
        ArgReadFunc func(argc, argv);
        foreach(func);
    }

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

    // Be careful about the position of BoundArgs. Generally variadic pack is at the end.
    template<typename F, int... S, typename... BoundArgs>
    auto dispatch(F func, Seq<S...>, BoundArgs... bndArgs)
    ->decltype(func(bndArgs..., std::get<S>(argTuple_)...)) {
        return func(bndArgs..., std::get<S>(argTuple_)...);
    }


    /**
     * foreach semantic: apply functor to each of the tuple element.
     *
     * http://stackoverflow.com/questions/1198260/iterate-over-tuple
     */
    template<size_t I = 0, typename F>
    typename std::enable_if<I <  sizeof...(ArgTypes), void>::type
    foreach(F& func) {
        func(std::get<I>(argTuple_));
        foreach<I+1, F>(func);
    }

    template<size_t I = 0, typename F>
    typename std::enable_if<I >= sizeof...(ArgTypes), void>::type
    foreach(F&) { }

    template<size_t I = 0, typename F>
    typename std::enable_if<I <  sizeof...(ArgTypes), void>::type
    foreach(F& func) const {
        func(std::get<I>(argTuple_));
        foreach<I+1, F>(func);
    }

    template<size_t I = 0, typename F>
    typename std::enable_if<I >= sizeof...(ArgTypes), void>::type
    foreach(F&) const { }

protected:
    /**
     * Functors for foreach.
     */

    struct StreamOutputFunc {
    private:
        std::ostream* os_;
        int idx_;

    public:
        StreamOutputFunc(std::ostream& os) : os_(&os), idx_(0) { }

        template<typename T>
        void operator()(const T& arg) {
            (*os_) << (idx_ == 0 ? "" : ", ") << arg;
            idx_++;
        }
    };

    struct ArgReadFunc {
    private:
        const int argc_;
        std::vector<char*> argv_;
        int idx_;

    public:
        ArgReadFunc(const int argc, char* const argv[])
            : argc_(argc), idx_(0)
        {
            for (int i = 0; i < argc_; i++) {
                argv_.push_back(argv[i]);
            }
        }

        template<typename T>
        void operator()(T& arg) {
            if (idx_ >= argc_) return;
            std::stringstream(argv_[idx_]) >> arg;
            idx_++;
        }
    };
};


#endif // HARNESS_H_

