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

    DegreeCount inDeg() const { return inDeg_; }
    DegreeCount outDeg() const { return outDeg_; }
    void inDegInc(const DegreeCount& d = 1u) {
        inDeg_ += d;
    }
    void outDegInc(const DegreeCount& d = 1u) {
        outDeg_ += d;
    }

    VertexDataType& data() { return data_; }
    const VertexDataType& data() const { return data_; }

private:
    const VertexIdx vid_;
    DegreeCount inDeg_;
    DegreeCount outDeg_;
    VertexDataType data_;

private:
    template<typename VDT, typename UDT, typename EWT>
    friend class GraphTile;

    template<typename... Args>
    explicit Vertex(const VertexIdx& vid, Args&&... args)
        : vid_(vid), inDeg_(0), outDeg_(0),
          data_(vid, std::forward<Args>(args)...)
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

    bool hasUpdate() const { return hasUpdate_; }

    UpdateType accUpdate() const { return accUpdate_; }

    /**
     * Add a new update, i.e., merge into accUpdate.
     */
    void updateNew(const UpdateType& update) {
        accUpdate_ += update;
        hasUpdate_ = true;
    }

    /**
     * Delete all updates, i.e., reset accUpdate.
     */
    void updateDelAll() {
        accUpdate_ = UpdateType();
        hasUpdate_ = false;
    }

    /**
     * When sync vertex degree with other tiles, clear the mirror vertex
     * acc degree after propagating to master vertex.
     */
    DegreeCount accDeg() const { return accDeg_; }
    void accDegDel() {
        accDeg_ = 0;
    }

protected:
    const VertexIdx vid_;
    const TileIdx masterTileId_;

    bool hasUpdate_;

    union {
    UpdateType accUpdate_;
    // Only used at vertex degree initialization. Must be cleared before real processing.
    DegreeCount accDeg_;
    };

protected:
    template<typename VDT, typename UDT, typename EWT>
    friend class GraphTile;

    MirrorVertex(const VertexIdx& vid, const TileIdx& masterTileId)
        : vid_(vid), masterTileId_(masterTileId), accDeg_(0)
          // accUpdate_ initialized after accDeg_ is used.
    {
        // Nothing else to do.
    }

    void accDegInc(const DegreeCount& d = 1u) {
        accDeg_ += d;
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
        : tid_(tid), vertices_(), edges_(), edgeSorted_(false), finalized_(false)
    {
        // Nothing else to do.
    }

    TileIdx tid() const { return tid_; }

    /* Vertices. */

    template<typename... Args>
    void vertexNew(const VertexIdx& vid, Args&&... args) {
        checkNotFinalized(__func__);
        auto vertex = Ptr<VertexType>(new VertexType(vid, std::forward<Args>(args)...));
        if (vertices_.insert( typename VertexMap::value_type(vid, vertex) ).second == false) {
            throw KeyInUseException(std::to_string(vid));
        }
    }

    Ptr<VertexType> vertex(const VertexIdx& vid) const {
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

    Ptr<MirrorVertexType> mirrorVertex(const VertexIdx& vid) const {
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
        checkNotFinalized(__func__);
        // Source vertex must be in this tile.
        if (vertices_.count(srcId) == 0) {
            throw RangeException(std::to_string(srcId));
        }
        // Destination vertex can be in different tile.
        if (dstTileId == tid_ && vertices_.count(dstId) == 0) {
            throw RangeException(std::to_string(dstId));
        }
        if (dstTileId != tid_ && mirrorVertices_.count(dstId) == 0) {
            // Create mirror vertex if destination vertex is in different tile.
            auto mirrorVertex = Ptr<MirrorVertexType>(new MirrorVertexType(dstId, dstTileId));
            mirrorVertices_.insert( typename MirrorVertexMap::value_type(dstId, mirrorVertex) );
        }
        // Repeating edges with the same srcId and dstId are accepted.
        // Use move constructor.
        edges_.push_back(EdgeType(srcId, dstId, weight));
        edgeSorted_ &= EdgeType::lessFunc(edges_[edges_.size()-2], edges_[edges_.size()-1]);
        // Increment degree.
        vertex(srcId)->outDegInc();
        if (dstTileId != tid_) {
            mirrorVertex(dstId)->accDegInc();
        } else {
            vertex(dstId)->inDegInc();
        }
    }

    bool edgeSorted() const { return edgeSorted_; }
    void edgeSortedIs(bool sorted) {
        if (!edgeSorted_ && sorted) {
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

    bool finalized() const { return finalized_; }
    void finalizedIs(const bool finalized) {
        if (!finalized_ && finalized) {
            // Finalize the graph tile.

            // Sort edge list.
            edgeSortedIs(true);

            // Check mirror vertex acc degree has been propagated to other tiles and cleared.
            for (auto mvIter = mirrorVertexIter(); mvIter != mirrorVertexIterEnd(); ++mvIter) {
                const auto mv = mvIter->second;
                if (mv->accDeg() != 0) {
                    throw PermissionException("Cannot finalize graph tile " + std::to_string(tid_)
                            + " due to uncleared mirror vertex " + std::to_string(mv->vid())
                            + " acc degree.");
                }
                // Reset application update.
                mv->updateDelAll();
            }

        }
        finalized_ = finalized;
    }

private:
    const TileIdx tid_;

    VertexMap vertices_;
    EdgeList edges_;

    MirrorVertexMap mirrorVertices_;

    bool edgeSorted_;

    /**
     * Finalize graph tile to prevent further changes on graph structure.
     * Any mutator to add/delete vertex/edge is not allowed after finalizing,
     * unless explicitly de-finalize. However, mutators that do not change
     * graph topology are allowed, e.g, edgeSortedIs().
     */
    bool finalized_;

private:
    void checkNotFinalized(const string& funcName) {
        if (finalized_) {
            throw PermissionException(funcName + ": Graph tile has already been finalized.");
        }
    }

    GraphTile(const GraphTile&) = delete;
    GraphTile& operator=(const GraphTile&) = delete;
    GraphTile(GraphTile&&) = delete;
    GraphTile& operator=(GraphTile&&) = delete;
    bool operator==(const GraphTile&) const = delete;

};

} // namespace GraphGASLite

#endif // GRAPH_H_
