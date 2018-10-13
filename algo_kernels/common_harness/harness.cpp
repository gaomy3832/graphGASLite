#include <fstream>
#include <iostream>
#include <sstream>
#include "engine.h"
#include "graph_io_util.h"

// Kernel harness header.
#include "kernel_harness.h"


int main(int argc, char* argv[]) {

    /* Parse arguments. */

    size_t threadCount;
    size_t graphTileCount;
    uint64_t maxIters;
    uint32_t numParts;
    bool undirected;

    std::string edgelistFile;
    std::string partitionFile;
    std::string outputFile;

    AppArgs appArgs;

    int argRet = algoKernelArgs(argc, argv,
            threadCount, graphTileCount, maxIters, numParts, undirected,
            edgelistFile, partitionFile, outputFile, appArgs);

    if (argRet) {
        algoKernelArgsPrintHelp(appName, appArgs);
        return argRet;
    }

    /* Make engine and load input. */

    GraphGASLite::Engine<Graph> engine;
    engine.graphTileIs(GraphGASLite::GraphIOUtil::graphTilesFromEdgeList<Graph>(
                threadCount, edgelistFile, partitionFile, 1, undirected, graphTileCount/threadCount, true));

    std::cout << "Graph loaded from " << edgelistFile <<
        (partitionFile.empty() ? "" : string(" and ") + partitionFile) <<
        " with " << graphTileCount << " graph tiles, " <<
        "into " << threadCount << " tiles." <<
        " Treated as " << (undirected ? "undirected" : "directed") << " graph." <<
        std::endl;

    /* Make algorithm kernel. */

    auto kernel = appArgs.algoKernel<Kernel>(appName);
    kernel->verboseIs(true);
    kernel->maxItersIs(maxIters);
    kernel->numPartsIs(numParts);
    engine.algoKernelNew(kernel);

    std::cout << "Algorithm kernel named " << appName <<
        " is " << algoKernelTagName(kernel->tag()) << ", " <<
        "with max iterations " << maxIters << " and number of partitions " << numParts << "." <<
        std::endl;

    std::cout << "Application parameters: " << appArgs << "." << std::endl;

    /* Run. */

    engine();

    /* Output. */
#ifdef VDATA
    if (!outputFile.empty()) {
        std::cout << "Output to " << outputFile << "." << std::endl;
        std::ofstream ofs(outputFile);
        for (size_t tid = 0; tid < threadCount; tid++) {
            auto g = engine.graphTile(tid);
            for (auto vIter = g->vertexIter(); vIter != g->vertexIterEnd(); ++vIter) {
                auto v = vIter->second;
                ofs << v->vid() << "\t" << VDATA(v->data()) << std::endl;
            }
        }
        ofs.close();
    }
#endif

    return 0;
}

