#include <emscripten/bind.h>

#include "utils.h"
#include "parallel.h"
#include "NeighborIndex.h"
#include "qdtsne/qdtsne.hpp"

#include <vector>
#include <cmath>
#include <chrono>
#include <random>
#include <iostream>

/**
 * @file run_tsne.cpp
 *
 * @brief Visualize cells with t-SNE.
 */

/**
 * @brief Status of the t-SNE algorithm.
 *
 * This is a wrapper around the similarly named `Status` object from the [**qdtsne**](https://github.com/LTLA/qdtsne) library.
 * The general idea is to create this object via `initialize_tsne()` before repeatedly calling `run_tsne()` to obtain updates.
 */
struct TsneStatus {
    /**
     * @cond
     */
    typedef qdtsne::Tsne<>::Status<int> Status;

    TsneStatus(Status s) : status(std::move(s)) {}

    Status status;
    /**
     * @endcond
     */

    /**
     * @return Number of iterations run so far.
     */
    int iterations () const {
        return status.iteration();
    }

    /**
     * @return A deep copy of this object.
     */
    TsneStatus deepcopy() const {
        return TsneStatus(status);
    }

    /**
     * @return Number of observations in the dataset.
     */
    int num_obs() const {
        return status.nobs();
    }
};

/**
 * Initialize the t-SNE from a pre-built nearest neighbor index.
 *
 * @param index A pre-built nearest neighbor index, usually generated by `build_neighbor_index()`.
 * @param perplexity t-SNE perplexity, controlling the trade-off between preservation of local and global structure.
 * Larger values focus on global structure more than the local structure.
 * @param[out] Y Offset to a 2-by-`nc` array containing the initial coordinates.
 * Each row corresponds to a dimension, each column corresponds to a cell, and the matrix is in column-major format.
 * This is filled with the first two rows of `mat`, i.e., the first and second PCs.
 *
 * @return A `TsneStatus` object that can be passed to `run_tsne()` to create 
 */
TsneStatus initialize_tsne(const NeighborResults& neighbors, double perplexity) {
    qdtsne::Tsne factory;
    factory.set_perplexity(perplexity);
    return TsneStatus(factory.template initialize<>(neighbors.neighbors));
}

/**
 * Randomize the starting t-SNE coordinates.
 * 
 * @param n Number of observations.
 * @param[out] Y Offset to an array of length `n * 2`.
 * @param seed Random seed for the PRNG.
 *
 * @return `Y` is filled with random draws from a Normal distribution.
 */
void randomize_tsne_start(size_t n, uintptr_t Y, int seed) {
    qdtsne::initialize_random(reinterpret_cast<double*>(Y), n, seed);
    return;
}

/**
 * @param perplexity Desired t-SNE perplexity.
 * @return Number of neighbors corresponding to `perplexity`.
 */
int perplexity_to_k(double perplexity) {
    return std::ceil(perplexity * 3);
}

/**
 * Run the t-SNE from an initialized `TsneStatus` object.
 *
 * @param status A `TsneStatus` object created by `initialize_status()`.
 * @param runtime Number of milliseconds to run before returning. 
 * Iterations are performed until the specified `runtime` is exceeded.
 * @param maxiter Maximum number of iterations to perform.
 * The function will return even if `runtime` has not been exceeded.
 * @param[in, out] Y Offset to a two-dimensional array containing the initial coordinates.
 * Each row corresponds to a dimension, each column corresponds to a cell, and the matrix is in column-major format.
 * On output, this will be filled with the updated coordinates.
 *
 * @return `Y` and `TsneStatus` are updated with the latest results.
 */
void run_tsne(TsneStatus& status, int runtime, int maxiter, uintptr_t Y) {
    qdtsne::Tsne factory;
    double* ptr = reinterpret_cast<double*>(Y);
    int iter = status.iterations();


    auto end = std::chrono::steady_clock::now() + std::chrono::milliseconds(runtime);
    do {
        ++iter;
        factory.set_max_iter(iter).run(status.status, ptr);
    } while (iter < maxiter && std::chrono::steady_clock::now() < end);

    return;
}
    
/**
 * @cond
 */
EMSCRIPTEN_BINDINGS(run_tsne) {
    emscripten::function("perplexity_to_k", &perplexity_to_k);

    emscripten::function("initialize_tsne", &initialize_tsne);

    emscripten::function("randomize_tsne_start", &randomize_tsne_start);

    emscripten::function("run_tsne", &run_tsne);

    emscripten::class_<TsneStatus>("TsneStatus")
        .function("iterations", &TsneStatus::iterations)
        .function("deepcopy", &TsneStatus::deepcopy)
        .function("num_obs", &TsneStatus::num_obs);
}
/**
 * @endcond
 */

