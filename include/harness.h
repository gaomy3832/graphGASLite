#ifndef HARNESS_H_
#define HARNESS_H_

#include <iostream>
#include <iomanip>
#include <sstream>
#include <unistd.h>

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

#endif // HARNESS_H_

