#ifndef GRAPH_IO_UTIL_H_
#define GRAPH_IO_UTIL_H_

#include <fstream>
#include <iterator>
#include <sstream>
#include <vector>
#include "graph.h"

template<typename GraphTileType>
class GraphIOUtil {
public:
    typedef typename GraphTileType::VertexType VertexType;
    typedef typename GraphTileType::EdgeType EdgeType;

    typedef typename EdgeType::WeightType EdgeWeightType;

public:
    GraphIOUtil(const EdgeWeightType& defaultWeight, const uint32_t tileMergeFactor = 1)
        : defaultWeight_(defaultWeight), tileMergeFactor_(tileMergeFactor)
    {
        // Nothing else to do.
    }

    /**
     * The default edge weight value.
     *
     * If no weight is given in the edge list file, then this value is used.
     */
    inline EdgeWeightType defaultWeight() const { return defaultWeight_; }

    /**
     * The factor for tile merge.
     *
     * The actual tile index of a vertex will be the index in the partition file
     * divided by this factor.
     */
    inline uint32_t tileMergeFactor() const { return tileMergeFactor_; }

    /**
     * Read graph topology from edge list file (and partition file). Partition
     * file name can be empty string, which means the graph is not partitioned
     * (only one tile).
     *
     * The tile vector must be sized and allocated before calling this function.
     *
     * Other arguments are used to construct the vertex.
     */
    template<typename... Args>
    void inputIs(std::vector< Ptr<GraphTileType> >& tiles,
            const string& edgeListFileName, const string& partitionFileName,
            Args&&... vertexArgs);

    /**
     * Write graph data to file.
     *
     * Default is doing nothing.
     */
    virtual void outputIs(std::vector< Ptr<GraphTileType> >& tiles,
            const string& outFileName) { }

protected:
    const EdgeWeightType defaultWeight_;
    const uint32_t tileMergeFactor_;

protected:
    /**
     * Get the next line until a non-commented, non-empty line.
     */
    inline static std::istream& nextEffectiveLine(std::istream& input, string& line) {
        do {
            std::getline(input, line);
        } while (input && (line.empty() || line.at(0) == '#'));
        return input;
    }

};


template<typename GraphTileType>
template<typename... Args>
void GraphIOUtil<GraphTileType>::inputIs(std::vector< Ptr<GraphTileType> >& tiles,
        const string& edgeListFileName, const string& partitionFileName,
        Args&&... vertexArgs) {

    try{
        auto tileCount = tiles.size();
        bool partitioned = (tileCount != 1);

        // Read vertices and their partitioned tile number, build the map.
        std::unordered_map< VertexIdx, TileIdx, std::hash<VertexIdx::Type> > tidMap;
        if (partitioned) {
            if (partitionFileName.empty()) {
                throw FileException(partitionFileName);
            }
            std::ifstream infile(partitionFileName, std::ifstream::in);
            if (!infile.is_open()) {
                throw FileException(partitionFileName);
            }
            string line;
            while (nextEffectiveLine(infile, line)) {
                std::istringstream iss(line);
                uint64_t vid = 0;
                uint32_t tid = 0;
                // Line format: <vid> <tid>
                if (!(iss >> vid >> tid)) {
                    throw FileException(partitionFileName);
                }
                // Merge tiles.
                tid /= tileMergeFactor();
                if (tid >= tileCount) {
                    throw RangeException(std::to_string(tid));
                }

                if (tidMap.emplace(vid, tid).second == false) {
                    throw KeyInUseException(std::to_string(vid));
                }
            }
        }

        auto vertexTileIdx = [&tidMap, partitioned](const VertexIdx& vid) {
            if (partitioned && tidMap.count(vid) != 0) {
                return tidMap.at(vid);
            } else {
                return TileIdx(0);
            }
        };

        // Read edge list file, build the graph tiles.
        if (edgeListFileName.empty()) {
            throw FileException(edgeListFileName);
        }
        std::ifstream infile(edgeListFileName, std::ifstream::in);
        if (!infile.is_open()) {
            throw FileException(edgeListFileName);
        }
        string line;
        while (nextEffectiveLine(infile, line)) {
            std::istringstream iss(line);
            uint64_t srcId = 0;
            uint64_t dstId = 0;
            EdgeWeightType weight;
            // Line format: <srcId> <dstId> [weight]
            if (!(iss >> srcId >> dstId)) {
                throw FileException(partitionFileName);
            }
            if (!(iss >> weight)) {
                weight = defaultWeight();
            }
            // Get corresponding tile and add vertex.
            const auto srcTid = vertexTileIdx(srcId);
            const auto dstTid = vertexTileIdx(dstId);
            if (!tiles[srcTid]->vertex(srcId)) {
                tiles[srcTid]->vertexNew(srcId, std::forward<Args>(vertexArgs)...);
            }
            if (!tiles[dstTid]->vertex(dstId)) {
                tiles[dstTid]->vertexNew(dstId, std::forward<Args>(vertexArgs)...);
            }
            // Add edge.
            tiles[srcTid]->edgeNew(srcId, dstId, dstTid, weight);
        }

        // Sort the edges in each tile to increase locality.
        for (auto t : tiles) {
            t->edgeSortedIs(true);
        }

    } catch (...) {
        throw FileException("Invalid format in graph topology input files.");
    }
}

#endif // GRAPH_UTIL_H_
