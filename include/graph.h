#ifndef GRAPH_H_
#define GRAPH_H_

#include <algorithm>
#include <unordered_map>
#include <vector>
#include "common.h"

class VertexIdxRepType;
typedef IndexType<uint64_t, VertexIdxRepType> VertexIdx;

class EdgeIdxRepType;
typedef IndexType<uint64_t, EdgeIdxRepType> EdgeIdx;

class TileIdxRepType;
typedef IndexType<uint64_t, TileIdxRepType> TileIdx;

template<typename VertexDataType, typename EdgeWeightType>
class GraphTile;

template<typename VertexDataType>
class Vertex {
public:
    typedef VertexDataType DataType;

    VertexIdx vid() const { return vid_; }

    VertexDataType& data() { return data_; }
    const VertexDataType& data() const { return data_; }

private:
    const VertexIdx vid_;
    VertexDataType data_;

private:
    template<typename VDT, typename EWT>
    friend class GraphTile;

    template<typename... Args>
    explicit Vertex(const VertexIdx& vid, Args&&... args)
        : vid_(vid), data_(vid, args...)
    {
        // Nothing else to do.
    }

    Vertex(const Vertex&) = delete;
    Vertex& operator=(const Vertex&) = delete;
    bool operator==(const Vertex&) const = delete;
};

template<typename EdgeWeightType = uint32_t>
class Edge {
public:
    typedef EdgeWeightType WeightType;

    /**
     * ``Less-than'' function used to sort edges, first source index, then dest index.
     *
     * Avoid overloading ==, <, etc..
     */
    static bool lessFunc(const Edge& e1, const Edge& e2) {
        if (e1.srcId_ == e2.srcId_) return e1.dstId_ < e2.dstId_;
        return e1.srcId_ < e2.srcId_;
    }

    VertexIdx srcId() const { return srcId_; }
    VertexIdx dstId() const { return dstId_; }

    /**
     * Tile index for destination vertex.
     *
     * This imbalance comes from the edge affinity: edge belongs to the tile of
     * its source vertex. So edge needs to know where is its destination vertex.
     */
    TileIdx dstTileId() const { return dstTileId_; }

    EdgeWeightType weight() const { return weight_; }
    void weightIs(const EdgeWeightType& weight) {
        weight_ = weight;
    }

private:
    const VertexIdx srcId_;
    const VertexIdx dstId_;
    const TileIdx dstTileId_;
    EdgeWeightType weight_;

private:
    template<typename VDT, typename EWT>
    friend class GraphTile;

    Edge(const VertexIdx& srcId, const VertexIdx& dstId, const TileIdx& dstTileId, const EdgeWeightType& weight)
        : srcId_(srcId), dstId_(dstId), dstTileId_(dstTileId), weight_(weight)
    {
        // Nothing else to do.
    }

    Edge(const Edge&) = delete;
    Edge& operator=(const Edge&) = delete;
    bool operator==(const Edge&) const = delete;
};


template<typename VertexDataType, typename EdgeWeightType = uint32_t>
class GraphTile {
public:
    typedef Vertex<VertexDataType> VertexType;
    typedef Edge<EdgeWeightType> EdgeType;

    typedef std::unordered_map< VertexIdx, Ptr<VertexType>, std::hash<VertexIdx::Type> > VertexMap;
    typedef std::vector< EdgeType > EdgeList;

    typedef typename EdgeList::iterator EdgeIter;
    typedef typename EdgeList::const_iterator EdgeConstIter;

public:
    explicit GraphTile(const TileIdx& tid)
        : tid_(tid), vertices_(), edges_(), edgeSorted_(false)
    {
        // Nothing else to do.
    }

    TileIdx tid() const { return tid_; }

    /* Vertices. */

    template<typename... Args>
    void vertexNew(const VertexIdx& vid, Args&&... args) {
        auto vertex = Ptr<VertexType>(new VertexType(vid, args...));
        if (vertices_.insert( typename VertexMap::value_type(vid, vertex) ).second == false) {
            throw KeyInUseException(std::to_string(vid));
        }
    }

    Ptr<VertexType> vertex(const VertexIdx& vid) {
        auto it = vertices_.find(vid);
        if (it != vertices_.end()) {
            return it->second;
        } else {
            return nullptr;
        }
    }

    size_t vertexCount() const { return vertices_.size(); }

    /* Edges. */

    void edgeNew(const VertexIdx& srcId, const VertexIdx& dstId, const TileIdx& dstTileId, const EdgeWeightType& weight) {
        // Source vertex must be in this tile.
        if (vertices_.count(srcId) == 0) {
            throw RangeException(std::to_string(srcId));
        }
        // Destination vertex can be in different tile.
        if (dstTileId == tid_ && vertices_.count(dstId) == 0) {
            throw RangeException(std::to_string(dstId));
        }
        // Repeating edges with the same srcId and dstId are accepted.
        edges_.emplace_back(srcId, dstId, dstTileId, weight);
        edgeSorted_ &= EdgeType::lessFunc(edges_[edges_.size()-2], edges_[edges_.size()-1]);
    }

    bool edgeSorted() const { return edgeSorted_; }
    void edgeSortedIs(bool sorted) {
        if (sorted) {
            std::sort(edges_.begin(), edges_.end(), EdgeType::lessFunc);
            edgeSorted_ = true;
        }
    }

    inline EdgeConstIter edgeIter() const {
        return edges_.cbegin();
    }
    inline EdgeConstIter edgeIterEnd() const {
        return edges_.cend();
    }

    inline EdgeIter edgeIter() {
        return edges_.begin();
    }
    inline EdgeIter edgeIterEnd() {
        return edges_.end();
    }

    size_t edgeCount() const { return edges_.size(); }

private:
    const TileIdx tid_;

    VertexMap vertices_;
    EdgeList edges_;

    bool edgeSorted_;

private:
    GraphTile(const GraphTile&) = delete;
    GraphTile& operator=(const GraphTile&) = delete;
    bool operator==(const GraphTile&) const = delete;

};

#endif // GRAPH_H_
