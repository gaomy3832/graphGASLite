#include "gtest/gtest.h"
#include "graph.h"
#include "graph_io_util.h"
#include "test_graph_types.h"

class GraphTest : public ::testing::Test {
public:
    typedef TestGraphTile::VertexType TestVertexType;

protected:
    virtual void SetUp() {
        graphs_ = GraphIOUtil::graphTilesFromEdgeList<TestGraphTile>(
                    2, "test_graphs/small.dat", "test_graphs/small.part", 0, 1, 0);
    }

    std::vector<Ptr<TestGraphTile>> graphs_;
};

TEST_F(GraphTest, tid) {
    ASSERT_EQ(0, static_cast<TileIdx::Type>(graphs_[0]->tid()));
    ASSERT_EQ(1, static_cast<TileIdx::Type>(graphs_[1]->tid()));
}

TEST_F(GraphTest, vertex) {
    ASSERT_EQ(0, graphs_[0]->vertex(0)->vid());
}

TEST_F(GraphTest, vertexNull) {
    ASSERT_EQ(nullptr, graphs_[0]->vertex(11));
}

TEST_F(GraphTest, vertexCount) {
    ASSERT_EQ(2, graphs_[0]->vertexCount());
    ASSERT_EQ(2, graphs_[1]->vertexCount());
}

TEST_F(GraphTest, vertexNew) {
    auto count = graphs_[0]->vertexCount();
    graphs_[0]->vertexNew(10, 0);
    ASSERT_EQ(count + 1, graphs_[0]->vertexCount());
}

TEST_F(GraphTest, vertexNewKeyInUse) {
    auto count = graphs_[0]->vertexCount();
    try {
        graphs_[0]->vertexNew(0, 0);
    } catch (KeyInUseException& e) {
        ASSERT_EQ(count, graphs_[0]->vertexCount());
        return;
    }

    // Never reached.
    ASSERT_TRUE(false);
}

TEST_F(GraphTest, vertexIter) {
    auto g = graphs_[0];
    for (TestGraphTile::VertexIter vIter = g->vertexIter(); vIter != g->vertexIterEnd(); ++vIter) {
        auto v = vIter->second;
        v->data().x_ = 1;
    }
    for (TestGraphTile::VertexConstIter vIter = g->vertexIter(); vIter != g->vertexIterEnd(); ++vIter) {
        const auto v = vIter->second;
        ASSERT_LT(std::abs(v->data().x_ - 1), 1e-3);
    }
}

TEST_F(GraphTest, mirrorVertex) {
    auto mv = graphs_[0]->mirrorVertex(2);
    ASSERT_EQ(2, mv->vid());
    ASSERT_EQ(1, mv->masterTileId());
    ASSERT_EQ(nullptr, graphs_[0]->mirrorVertex(0));
}

TEST_F(GraphTest, mirrorVertexIter) {
    auto g = graphs_[0];
    size_t count = 0;
    for (auto mvIter = g->mirrorVertexIter(); mvIter != g->mirrorVertexIterEnd(); ++mvIter) {
        auto mv = mvIter->second;
        ASSERT_EQ(1, mv->masterTileId());
        count++;
    }
    ASSERT_EQ(2, count);
}

TEST_F(GraphTest, edgeCount) {
    auto g = graphs_[0];
    ASSERT_EQ(3, g->edgeCount());
}

TEST_F(GraphTest, edgeIter) {
    auto g = graphs_[0];
    for (auto eIter = g->edgeIter(); eIter != g->edgeIterEnd(); ++eIter) {
        auto& e = *eIter;
        auto srcId = e.srcId();
        ASSERT_NE(nullptr, g->vertex(srcId));
    }
}

TEST_F(GraphTest, edgeNew) {
    auto g = graphs_[0];
    g->edgeNew(1, 0, 0, 10);

    auto eIter = g->edgeIter();
    for (; eIter != g->edgeIterEnd(); ++eIter) {
        auto& e = *eIter;
        if (e.weight() > 9) {
            ASSERT_EQ(1, e.srcId());
            ASSERT_EQ(0, e.dstId());
            break;
        }
    }
    // Found the new edge.
    ASSERT_FALSE(eIter == g->edgeIterEnd());
}

TEST_F(GraphTest, edgeSorted) {
    auto g = graphs_[0];
    g->edgeSortedIs(true);
    ASSERT_TRUE(g->edgeSorted());
    g->edgeNew(1, 0, 0, 10);
    ASSERT_FALSE(g->edgeSorted());
    g->edgeSortedIs(true);
    ASSERT_TRUE(g->edgeSorted());
}

