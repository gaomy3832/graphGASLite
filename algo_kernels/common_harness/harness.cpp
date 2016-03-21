#include <fstream>
#include <iostream>
#include <sstream>
#include "engine.h"
#include "graph_io_util.h"

// Kernel harness header.
#include "kernel_harness.h"

#ifdef VERTEX_ARGS
#define VERTEX_ARGS_WITH_COMMA ,VERTEX_ARGS
#else
#define VERTEX_ARGS_WITH_COMMA
#endif // VERTEX_ARGS


int main(int argc, char* argv[]) {

    /* Common arguments. */

    size_t threadCount;
    size_t graphTileCount;
    uint64_t maxIters;
    uint32_t numParts;

    auto optind = algoKernelComArg(argc, argv, threadCount, graphTileCount, maxIters, numParts);
    if (optind == -1) return 1;
    argc -= optind;
    argv += optind;

    /* Input/output path. */

    std::string edgelistFile("");
    std::string partitionFile("");
    std::string outputFile("");
    AppArgs appArgs;

    if (argc < 1) {
        std::cerr << "Must specify an input edge list file." << std::endl;
        usage(appName, appArgs.name());
        return 1;
    }
    edgelistFile = argv[0];
    if (argc > 1) {
        partitionFile = argv[1];
        if (argc > 2) {
            outputFile = argv[2];

            argc -= 3;
            argv += 3;
            appArgs.argIs(argc, argv);
        }
    }

    /* Make engine and load input. */

    GraphGASLite::Engine<Graph> engine;
    engine.graphTileIs(GraphGASLite::GraphIOUtil::graphTilesFromEdgeList<Graph>(
                threadCount, edgelistFile, partitionFile, 1, graphTileCount/threadCount, true
                VERTEX_ARGS_WITH_COMMA));

    std::cout << "Graph loaded from " << edgelistFile <<
        (partitionFile.empty() ? "" : string(" and ") + partitionFile) <<
        " with " << graphTileCount << " graph tiles, " <<
        "into " << threadCount << " tiles." <<
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

