#ifndef ALGO_KERNELS_EDGE_CENTRIC_ALS_ALS_H_
#define ALGO_KERNELS_EDGE_CENTRIC_ALS_ALS_H_

#include <algorithm>
#include <array>
#include <cstdlib>
#include <numeric>
#include "graph.h"
#include "algo_kernel.h"

/*
 * int dgesv(const enum ORDER ord, const int n, const int nrhs,
 *         double* a, const int lda, int* ipiv,
 *         double* b, const int ldb);
 * a: lda x n
 * b: ldb x nrhs
 * ipiv: pivot indices
 * lda/ldb: leading dimension
 * nrhs: number of right hand sides
 */
#ifdef USE_MKL
#include "mkl.h"
template<size_t N>
void sqmat_ldiv_vec(double* sqmat, double* vec) {
    lapack_int ipiv[N];
    LAPACKE_dgesv(LAPACK_ROW_MAJOR, N, 1, sqmat, N, ipiv, vec, 1);
}
#else // USE_MKL
#ifdef USE_ATLAS
#ifdef __cplusplus
extern "C"
{
#endif
#include "clapack.h"
#ifdef __cplusplus
}
#endif
template<size_t N>
void sqmat_ldiv_vec(double* sqmat, double* vec) {
    int ipiv[N];
    clapack_dgesv(CblasRowMajor, N, 1, sqmat, N, ipiv, vec, 1);
}
#endif // USE_ATLAS
#endif // USE_MKL

/*
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
static void veczero(Vec<R>& vec) { std::fill(vec.begin(), vec.end(), 0); }

template<size_t R>
static void matzero(Mat<R>& mat) { for (auto& vec : mat) veczero(vec); }

template<size_t R>
static void veccpy(Vec<R>& dst, const Vec<R>& src) {
    std::copy(src.begin(), src.end(), dst.begin());
}

template<size_t R>
static void matcpy(Mat<R>& dst, const Mat<R>& src) {
    for (size_t idx = 0; idx < R; idx++) veccpy(dst[idx], src[idx]);
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
    for (size_t idx = 0; idx < R; idx++) vecadd(dst[idx], mat1[idx], mat2[idx]);
}

template<size_t R>
static typename Vec<R>::value_type vecinprod(const Vec<R>& vec1, const Vec<R>& vec2) {
    constexpr typename Vec<R>::value_type val = 0;
    return std::inner_product(vec1.begin(), vec1.end(), vec2.begin(), val);
}

template<size_t R>
static typename Vec<R>::value_type vecnormsq(const Vec<R>& vec) {
    return vecinprod(vec, vec);
}

/*
 * Solve mat \ vec.
 */
template<size_t R>
void solve(const Mat<R>& mat, Vec<R>& vec) {
    double linmat[R*R];
    for (size_t idx = 0; idx < R; idx++)
        for (size_t jdx = 0; jdx < R; jdx++)
            linmat[idx * R + jdx] = mat[idx][jdx];
    double* linvec = vec.data();
    sqmat_ldiv_vec<R>(linmat, linvec);
}


/*
 * Graph types definitions.
 */
/*
 * Syntax and algorithm refer to:
 *   als.aaim08
 *   xstream x-stream/algorithms/als/als.hpp
 */
template<size_t R>
struct ALSData {
    Role role;
    Vec<R> features;   // ui, mj
    GraphGASLite::DegreeCount collected;
    Vec<R> vector;     // Vi, Vj
    Mat<R> matrix;     // Ai, Aj

    ALSData(const GraphGASLite::VertexIdx&, const Role role_ = Role::INVALID)
        : role(role_), collected(0)
    {
        veczero(features);
        veczero(vector);
        matzero(matrix);
    }
};

template<size_t R>
struct ALSUpdate {
    Vec<R> vector;         // Vi, Vj
    Mat<R> matrix;         // Ai, Aj

    ALSUpdate(const Vec<R>& vector_, const Mat<R>& matrix_)
        : vector(vector_), matrix(matrix_)
    {
        // Nothing else to do.
    }

    ALSUpdate() {
        veczero(vector);
        matzero(matrix);
    }

    ALSUpdate& operator+=(const ALSUpdate& update) {
        vecadd(vector, vector, update.vector);
        matadd(matrix, matrix, update.matrix);
        return *this;
    }
};


/*
 * Algorithm kernel definition.
 */
template<typename GraphTileType>
class ALSEdgeCentricAlgoKernel : public GraphGASLite::EdgeCentricAlgoKernel<GraphTileType> {
public:
    static Ptr<ALSEdgeCentricAlgoKernel> instanceNew(const string& name,
            const GraphGASLite::VertexIdx::Type boundary, const double lambda, const double tolerance,
            const GraphGASLite::IterCount::Type errEpoch) {
        return Ptr<ALSEdgeCentricAlgoKernel>(new ALSEdgeCentricAlgoKernel(name, boundary, lambda, tolerance, errEpoch));
    }

protected:
    typedef typename GraphTileType::UpdateType UpdateType;
    typedef typename GraphTileType::VertexType VertexType;
    typedef typename GraphTileType::EdgeType::WeightType EdgeWeightType;

    std::pair<UpdateType, bool> scatter(const GraphGASLite::IterCount& iter, Ptr<VertexType>& src, EdgeWeightType& weight) const {
        auto& data = src->data();

        std::pair<UpdateType, bool> ret;
        ret.second = false;

        /* The graph is bipartite. In even iteration, we solve the user side
         * (send movie features to user); and in odd iteration, we solve the
         * movie side (send user features to moive).
         */
        if (((iter.cnt() & 0x1) && data.role == Role::USER) ||
                (!(iter.cnt() & 0x1) && data.role == Role::MOVIE)) {
            auto& update = ret.first;
            auto& features = data.features;

            // vector <= ui * rij
            std::transform(features.begin(), features.end(), update.vector.begin(),
                    [weight](const double x){ return x * weight; });    // edge weight is rij
            // matrix <= ui * ui^T
            for (size_t idx = 0; idx < features.size(); idx++) {
                for (size_t jdx = 0; jdx < features.size(); jdx++) {
                    update.matrix[idx][jdx] = features[idx] * features[jdx];
                }
            }

            ret.second = true;
        }
        return ret;
    }

    bool gather(const GraphGASLite::IterCount&, Ptr<VertexType>& dst, const UpdateType& update) const {
        auto& data = dst->data();
        auto ideg = dst->inDeg();

        vecadd(data.vector, data.vector, update.vector);
        matadd(data.matrix, data.matrix, update.matrix);
        data.collected++;

        if (data.collected == ideg) {
            // matrix <= sum{ ui * ui^T } + lambda * n_ui * E
            for (size_t idx = 0; idx < data.matrix.size(); idx++) {
                data.matrix[idx][idx] += lambda_ * ideg;
            }

            // features <= matrix \ vector
            solve(data.matrix, data.vector);
            vecdiff(data.features, data.features, data.vector);
            bool converged = (std::abs(vecnormsq(data.features)) < tolerance_ * tolerance_);
            veccpy(data.features, data.vector);

            data.collected = 0;
            veczero(data.vector);
            matzero(data.matrix);

            return converged;
        }

        // Convergency is unknown until all updates are collected.
        return true;
    }

    void onAlgoKernelStart(Ptr<GraphTileType>& graph) const {
        // Specify vertex roles.
        for (auto vertexIter = graph->vertexIter(); vertexIter != graph->vertexIterEnd(); ++vertexIter) {
            auto& v = vertexIter->second;
            v->data().role = v->vid() < boundary_ ? Role::USER : Role::MOVIE;
        }

        // Initialize movie features.
        // First feature is average rating, others are small random numbers (between +/-5).
        for (auto edgeIter = graph->edgeIter(); edgeIter != graph->edgeIterEnd(); ++edgeIter) {
            auto& edge = *edgeIter;
            auto& data = graph->vertex(edge.srcId())->data();
            if (data.role == Role::MOVIE) {
                data.features[0] += edge.weight();
                data.collected += 1;
            }
        }
        for (auto vertexIter = graph->vertexIter(); vertexIter != graph->vertexIterEnd(); ++vertexIter) {
            auto& data = vertexIter->second->data();
            if (data.role == Role::MOVIE) {
                data.features[0] = data.features[0] / data.collected;
                std::generate(data.features.begin() + 1, data.features.end(),
                        []{ return (std::rand() % 10) - 5; });
                data.collected = 0;
            }
        }
    }

    void onIterationEnd(Ptr<GraphTileType>& graph, const GraphGASLite::IterCount& iter) const {
        if (errEpoch_ != 0 && iter % errEpoch_ == 0) {
            double err = 0;
            for (auto edgeIter = graph->edgeIter(); edgeIter != graph->edgeIterEnd(); ++edgeIter) {
                auto& edge = *edgeIter;
                const auto& srcFeatures = graph->vertex(edge.srcId())->data().features;
                const auto& dstFeatures = graph->vertex(edge.dstId())->data().features;
                double diff = edge.weight() - vecinprod(srcFeatures, dstFeatures);
                err += diff * diff;
            }
            // Each undirect edge has been counted twice for each direction.
            err /= 2;
            info("\tIteration %lu: error %lf", iter.cnt(), err);
        }
    }

protected:
    ALSEdgeCentricAlgoKernel(const string& name,
            const GraphGASLite::VertexIdx::Type boundary, const double lambda, const double tolerance,
            const GraphGASLite::IterCount::Type errEpoch)
        : GraphGASLite::EdgeCentricAlgoKernel<GraphTileType>(name),
          boundary_(boundary), lambda_(lambda), tolerance_(tolerance), errEpoch_(errEpoch)
    {
        // Nothing else to do.
    }

private:
    const GraphGASLite::VertexIdx boundary_;
    const double lambda_;
    const double tolerance_;
    const GraphGASLite::IterCount errEpoch_;
};

#endif // ALGO_KERNELS_EDGE_CENTRIC_ALS_ALS_H_

