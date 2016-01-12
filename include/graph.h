#ifndef GRAPH_H_
#define GRAPH_H_

#include <algorithm>
#include <unordered_map>
#include <vector>
#include "common.h"

namespace GraphGASLite {

class VertexIdxRepType;
typedef IndexType<uint64_t, VertexIdxRepType> VertexIdx;

class EdgeIdxRepType;
typedef IndexType<uint64_t, EdgeIdxRepType> EdgeIdx;

class TileIdxRepType;
typedef IndexType<uint64_t, TileIdxRepType> TileIdx;

class DegreeRepType;
typedef CountType<uint32_t, DegreeRepType> DegreeCount;

template<typename VertexDataType, typename UpdateDataType, typename EdgeWeightType>
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
    template<typename VDT, typename UDT, typename EWT>
    friend class GraphTile;

    template<typename... Args>
    explicit Vertex(const VertexIdx& vid, Args&&... args)
        : vid_(vid), data_(vid, std::forward<Args>(args)...)
    {
        // Nothing else to do.
    }

    Vertex(const Vertex&) = delete;
    Vertex& operator=(const Vertex&) = delete;
    Vertex(Vertex&&) = delete;
    Vertex& operator=(Vertex&&) = delete;
    bool operator==(const Vertex&) const = delete;
};

template<typename UpdateDataType>
class MirrorVertex {
public:
    typedef UpdateDataType UpdateType;

    VertexIdx vid() const { return vid_; }

    TileIdx masterTileId() const { return masterTileId_; }

    UpdateType accUpdate() const { return accUpdate_; }

    /**
     * Add a new update, i.e., merge into accUpdate.
     */
    void updateNew(const UpdateType& update) {
        accUpdate_ += update;
    }

    /**
     * Delete all updates, i.e., reset accUpdate.
     */
    void updateDelAll() {
        accUpdate_ = UpdateType();
    }

protected:
    const VertexIdx vid_;
    const TileIdx masterTileId_;
    UpdateType accUpdate_;

protected:
    template<typename VDT, typename UDT, typename EWT>
    friend class GraphTile;

    template<typename... Args>
    MirrorVertex(const VertexIdx& vid, const TileIdx& masterTileId, Args&&... args)
        : vid_(vid), masterTileId_(masterTileId), accUpdate_(std::forward<Args>(args)...)
    {
        // Nothing else to do.
    }

    MirrorVertex(const MirrorVertex&) = delete;
    MirrorVertex& operator=(const MirrorVertex&) = delete;
    MirrorVertex(MirrorVertex&&) = delete;
    MirrorVertex& operator=(MirrorVertex&&) = delete;
    bool operator==(const MirrorVertex&) const = delete;
};

template<typename EdgeWeightType = uint32_t>
class Edge {
public:
    typedef EdgeWeightType WeightType;

    VertexIdx srcId() const { return srcId_; }
    VertexIdx dstId() const { return dstId_; }

    EdgeWeightType weight() const { return weight_; }
    void weightIs(const EdgeWeightType& weight) {
        weight_ = weight;
    }
    EdgeWeightType& weight() { return weight_; }

private:
    VertexIdx srcId_;
    VertexIdx dstId_;
    EdgeWeightType weight_;

private:
    template<typename VDT, typename UDT, typename EWT>
    friend class GraphTile;

    Edge(const VertexIdx& srcId, const VertexIdx& dstId, const EdgeWeightType& weight)
        : srcId_(srcId), dstId_(dstId), weight_(weight)
    {
        // Nothing else to do.
    }

    /**
     * ``Less-than'' function used to sort edges, first source index, then dest index.
     *
     * Avoid overloading ==, <, etc..
     */
    static bool lessFunc(const Edge& e1, const Edge& e2) {
        if (e1.srcId_ == e2.srcId_) return e1.dstId_ < e2.dstId_;
        return e1.srcId_ < e2.srcId_;
    }

    Edge(const Edge&) = delete;
    Edge& operator=(const Edge&) = delete;
    bool operator==(const Edge&) const = delete;

public:
    /**
     * Move-assignable and move-constructible, to allow sorting.
     */
    Edge(Edge&&) = default;
    Edge& operator=(Edge&&) = default;
};


template<typename VertexDataType, typename UpdateDataType, typename EdgeWeightType = uint32_t>
class GraphTile {
public:
    typedef UpdateDataType UpdateType;

    typedef Vertex<VertexDataType> VertexType;
    typedef Edge<EdgeWeightType> EdgeType;
    typedef MirrorVertex<UpdateDataType> MirrorVertexType;

    typedef std::unordered_map< VertexIdx, Ptr<VertexType>, std::hash<VertexIdx::Type> > VertexMap;
    typedef std::vector< EdgeType > EdgeList;
    typedef std::unordered_map< VertexIdx, Ptr<MirrorVertexType>, std::hash<VertexIdx::Type> > MirrorVertexMap;

    typedef typename VertexMap::iterator VertexIter;
    typedef typename VertexMap::const_iterator VertexConstIter;
    typedef typename EdgeList::iterator EdgeIter;
    typedef typename EdgeList::const_iterator EdgeConstIter;
    typedef typename MirrorVertexMap::iterator MirrorVertexIter;
    typedef typename MirrorVertexMap::const_iterator MirrorVertexConstIter;

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
        auto vertex = Ptr<VertexType>(new VertexType(vid, std::forward<Args>(args)...));
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

    inline VertexConstIter vertexIter() const {
        return vertices_.cbegin();
    }
    inline VertexConstIter vertexIterEnd() const {
        return vertices_.cend();
    }

    inline VertexIter vertexIter() {
        return vertices_.begin();
    }
    inline VertexIter vertexIterEnd() {
        return vertices_.end();
    }

    /* Mirror vertices. */

    Ptr<MirrorVertexType> mirrorVertex(const VertexIdx& vid) {
        auto it = mirrorVertices_.find(vid);
        if (it != mirrorVertices_.end()) {
            return it->second;
        } else {
            return nullptr;
        }
    }

    inline MirrorVertexConstIter mirrorVertexIter() const {
        return mirrorVertices_.cbegin();
    }
    inline MirrorVertexConstIter mirrorVertexIterEnd() const {
        return mirrorVertices_.cend();
    }

    inline MirrorVertexIter mirrorVertexIter() {
        return mirrorVertices_.begin();
    }
    inline MirrorVertexIter mirrorVertexIterEnd() {
        return mirrorVertices_.end();
    }

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
        if (dstTileId != tid_) {
            // Create mirror vertex if destination vertex is in different tile.
            auto mirrorVertex = Ptr<MirrorVertexType>(new MirrorVertexType(dstId, dstTileId));
            mirrorVertices_.insert( typename MirrorVertexMap::value_type(dstId, mirrorVertex) );
        }
        // Repeating edges with the same srcId and dstId are accepted.
        // Use move constructor.
        edges_.push_back(EdgeType(srcId, dstId, weight));
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

    MirrorVertexMap mirrorVertices_;

    bool edgeSorted_;

private:
    GraphTile(const GraphTile&) = delete;
    GraphTile& operator=(const GraphTile&) = delete;
    GraphTile(GraphTile&&) = delete;
    GraphTile& operator=(GraphTile&&) = delete;
    bool operator==(const GraphTile&) const = delete;

};

} // namespace GraphGASLite

#endif // GRAPH_H_
