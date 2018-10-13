#ifndef ALGO_KERNELS_EDGE_CENTRIC_ALS_ALS_H_
#define ALGO_KERNELS_EDGE_CENTRIC_ALS_ALS_H_

#include <algorithm>
#include <array>
#include <cstdlib>
#include <numeric>
#include "graph.h"
#include "algo_kernel.h"

#ifdef __cplusplus
extern "C"
{
#endif
#include "clapack.h"
#ifdef __cplusplus
}
#endif

namespace GraphGASLite {

/*
 * Graph types definition.
 *
 * Define ALS() in graph data and update to get ALSData and ALSUpdate.
 */
template<size_t>
struct ALSData;

template<size_t>
struct ALSUpdate;

/**
 * Role of vertex.
 */
enum class Role {
    USER,
    MOVIE,
    INVALID,
};

/*
 * Vector and Matrix types.
 */
template<size_t R>
using Vec = std::array<double, R>;

template<size_t R>
using Mat = std::array<Vec<R>, R>;

template<size_t R>
static void veczero(Vec<R>& vec) {
    std::fill(vec.begin(), vec.end(), 0);
}

template<size_t R>
static void matzero(Mat<R>& mat) {
    for (auto& vec : mat) {
        veczero(vec);
    }
}

template<size_t R>
static void veccpy(Vec<R>& dst, const Vec<R>& src) {
    std::copy(src.begin(), src.end(), dst.begin());
}

template<size_t R>
static void matcpy(Mat<R>& dst, const Mat<R>& src) {
    for (size_t idx = 0; idx < R; idx++) {
        veccpy(dst[idx], src[idx]);
    }
}

template<size_t R>
static void vecadd(Vec<R>& dst, const Vec<R>& vec1, const Vec<R>& vec2) {
    std::transform(vec1.begin(), vec1.end(), vec2.begin(), dst.begin(),
            [](const typename Vec<R>::value_type v1, const typename Vec<R>::value_type v2){ return v1 + v2; });
}

template<size_t R>
static void vecdiff(Vec<R>& dst, const Vec<R>& vec1, const Vec<R>& vec2) {
    std::transform(vec1.begin(), vec1.end(), vec2.begin(), dst.begin(),
            [](const typename Vec<R>::value_type v1, const typename Vec<R>::value_type v2){ return v1 - v2; });
}

template<size_t R>
static void matadd(Mat<R>& dst, const Mat<R>& mat1, const Mat<R>& mat2) {
    for (size_t idx = 0; idx < R; idx++) {
        vecadd(dst[idx], mat1[idx], mat2[idx]);
    }
}

template<size_t R>
static typename Vec<R>::value_type vecnormsq(const Vec<R>& vec) {
    return std::inner_product(vec.begin(), vec.end(), vec.begin(), 0);
}

/*
 * Solve mat \ vec using LAPACK.
 *
 * int clapack_dgesv(const enum CBLAS_ORDER Order, const int N, const int NRHS,
 *         double *A, const int lda, int *ipiv,
 *         double *B, const int ldb);
 * A: lda x N
 * B: ldb x nrhs
 * ipiv: pivot indices
 * ld: leading dimension, nrhs: number of right hand sides
 */
template<size_t R>
void solve(const Mat<R>& mat, Vec<R>& vec) {
    int ipiv[R];
    double linmat[R*R];
    for (size_t idx = 0; idx < R; idx++)
        for (size_t jdx = 0; jdx < R; jdx++)
            linmat[idx * R + jdx] = mat[idx][jdx];
    double* linvec = vec.data();
    clapack_dgesv(CblasRowMajor, R, 1, linmat, R, ipiv, linvec, R);
}


/*
 * Syntax and algorithm refer to:
 *   als.aaim08
 *   xstream x-stream/algorithms/als/als.hpp
 */
template<size_t R>
struct ALSData {
    Role role_;
    Vec<R> features_;   // ui, mj
    DegreeCount collected_;
    Vec<R> vector_;     // Vi, Vj
    Mat<R> matrix_;     // Ai, Aj

    ALSData(const VertexIdx&, const Role role = Role::INVALID)
        : role_(role), collected_(0)
    {
        veczero(features_);
        veczero(vector_);
        matzero(matrix_);
    }

    inline ALSData& ALS() { return *this; }
    inline const ALSData& ALS() const { return *this; }
};

template<size_t R>
struct ALSUpdate {
    Vec<R> vector_;         // Vi, Vj
    Mat<R> matrix_;         // Ai, Aj

    ALSUpdate(const Vec<R>& vector, const Mat<R>& matrix)
        : vector_(vector), matrix_(matrix)
    {
        // Nothing else to do.
    }

    ALSUpdate() {
        veczero(vector_);
        matzero(matrix_);
    }

    ALSUpdate& operator+=(const ALSUpdate& update) {
        vecadd(this->vector_, this->vector_, update.vector_);
        matadd(this->matrix_, this->matrix_, update.matrix_);
        return *this;
    }

    inline ALSUpdate& ALS() { return *this; }
    inline const ALSUpdate& ALS() const { return *this; }
};


/*
 * Algorithm kernel definition.
 */
template<typename GraphTileType>
class ALSEdgeCentricAlgoKernel : public EdgeCentricAlgoKernel<GraphTileType> {
public:
    static Ptr<ALSEdgeCentricAlgoKernel> instanceNew(const string& name,
            const double lambda, const double tolerance) {
        return Ptr<ALSEdgeCentricAlgoKernel>(new ALSEdgeCentricAlgoKernel(name, lambda, tolerance));
    }

protected:
    typedef typename GraphTileType::UpdateType UpdateType;
    typedef typename GraphTileType::VertexType VertexType;
    typedef typename GraphTileType::EdgeType::WeightType EdgeWeightType;

    std::pair<UpdateType, bool> scatter(const IterCount& iter, Ptr<VertexType>& src, EdgeWeightType& w) const {
        auto& dals = src->data().ALS();

        std::pair<UpdateType, bool> ret;
        ret.second = false;

        /* The graph is bipartite. In even iteration, we solve the user side
         * (send movie features to user); and in odd iteration, we solve the
         * movie side (send user features to moive).
         */
        if (((iter.cnt() & 0x1) && dals.role_ == Role::USER) ||
                (!(iter.cnt() & 0x1) && dals.role_ == Role::MOVIE)) {
            auto& update = ret.first;
            auto& features = dals.features_;

            // vector <= ui * rij
            std::transform(features.begin(), features.end(), update.vector_.begin(),
                    [w](const double x){ return x * w; });    // edge weight is rij
            // matrix <= ui * ui^T
            for (size_t idx = 0; idx < features.size(); idx++) {
                for (size_t jdx = 0; jdx < features.size(); jdx++) {
                    update.matrix_[idx][jdx] = features[idx] * features[jdx];
                }
            }

            ret.second = true;
        }
        return ret;
    }

    bool gather(const IterCount&, Ptr<VertexType>& dst, const UpdateType& update) const {
        auto& dals = dst->data().ALS();
        auto& uals = update.ALS();

        vecadd(dals.vector_, dals.vector_, uals.vector_);
        matadd(dals.matrix_, dals.matrix_, uals.matrix_);
        dals.collected_++;

        if (dals.collected_ == dst->inDeg()) {
            // matrix <= sum{ ui * ui^T } + lambda * n_ui * E
            for (size_t idx = 0; idx < dals.matrix_.size(); idx++) {
                dals.matrix_[idx][idx] += lambda_ * dst->inDeg();
            }

            // features <= matrix \ vector
            solve(dals.matrix_, dals.vector_);
            vecdiff(dals.features_, dals.features_, dals.vector_);
            bool converged = (std::abs(vecnormsq(dals.features_)) < tolerance_ * tolerance_);
            veccpy(dals.features_, dals.vector_);

            dals.collected_ = 0;
            veczero(dals.vector_);
            matzero(dals.matrix_);

            return converged;
        }

        // Convergency is unknown until all updates are collected.
        return true;
    }

    void onAlgoKernelStart(Ptr<GraphTileType>& graph) const {
        // Specify vertex roles.
        for (auto vertexIter = graph->vertexIter(); vertexIter != graph->vertexIterEnd(); ++vertexIter) {
            auto& v = vertexIter->second;
            v->data().ALS().role_ = v->vid() < 10000000 ? Role::USER : Role::MOVIE;
        }

        // Initialize movie features.
        // First feature is average rating, others are small random numbers (between +/-5).
        for (auto edgeIter = graph->edgeIter(); edgeIter != graph->edgeIterEnd(); ++edgeIter) {
            auto& edge = *edgeIter;
            auto src = graph->vertex(edge.srcId());
            if (src->data().ALS().role_ == Role::MOVIE) {
                src->data().ALS().features_[0] += edge.weight();
            }
        }
        for (auto vertexIter = graph->vertexIter(); vertexIter != graph->vertexIterEnd(); ++vertexIter) {
            auto& v = vertexIter->second;
            if (v->data().ALS().role_ == Role::MOVIE) {
                auto& features = v->data().ALS().features_;
                features[0] = features[0] / v->outDeg();
                std::generate(features.begin() + 1, features.end(),
                        []{ return (std::rand() % 10) - 5; });
            }
        }
    }

protected:
    ALSEdgeCentricAlgoKernel(const string& name, const double lambda, const double tolerance)
        : EdgeCentricAlgoKernel<GraphTileType>(name),
          lambda_(lambda), tolerance_(tolerance)
    {
        // Nothing else to do.
    }

private:
    const double lambda_;
    const double tolerance_;
};

} // namespace GraphGASLite

#endif // ALGO_KERNELS_EDGE_CENTRIC_ALS_ALS_H_

