//=======================================================================
// Copyright (c) 2014-2017 Baptiste Wicht
// Distributed under the terms of the MIT License.
// (See accompanying file LICENSE or copy at
//  http://opensource.org/licenses/MIT)
//=======================================================================

#pragma once

#ifdef ETL_CUDNN_MODE

#include "etl/impl/cublas/cuda.hpp"
#include "etl/impl/cudnn/cudnn.hpp"

#endif

namespace etl {

namespace impl {

namespace cudnn {

#ifdef ETL_CUDNN_MODE

/*!
 * \brief Create a CUDNN pooling descriptor for the given input matrix
 * \return a cudnn_wrapper around a created CUDNN filter tensor
 */
inline cudnn_wrapper<cudnnPoolingDescriptor_t> create_pooling_descriptor(cudnnPoolingMode_t mode, size_t c1, size_t c2, size_t s1, size_t s2, size_t p1, size_t p2){
    cudnnPoolingDescriptor_t pooling_desc;
    cudnn_check(cudnnCreatePoolingDescriptor(&pooling_desc));
    cudnn_check(cudnnSetPooling2dDescriptor(pooling_desc, mode, CUDNN_PROPAGATE_NAN, c1, c2, p1, p2, s1, s2));

    return cudnn_wrapper<cudnnPoolingDescriptor_t>{pooling_desc};
}

/*!
 * \brief Functor for 2D Max Pooling
 */
struct max_pool_2d {
    /*!
     * \brief Apply the functor on sub and store the result in m
     * \param sub The sub expression
     * \param m The storage matrix
     * \param c1 The first dimension pooling ratio
     * \param c2 The second dimension pooling ratio
     */
    template <typename X, typename Y, cpp_enable_if((decay_traits<X>::dimensions() < 5))>
    static void apply(const X& x, Y&& y, size_t c1, size_t c2, size_t s1, size_t s2, size_t p1, size_t p2) {
        using type = std::remove_const_t<value_t<X>>;

        decltype(auto) handle = start_cudnn();

        auto pooling_desc = create_pooling_descriptor(CUDNN_POOLING_MAX, c1, c2, s1, s2, p1, p2);

        auto x_tensor = create_tensor(x);
        auto y_tensor = create_tensor(y);

        type alpha[] = {1.0f};
        type beta[] = {0.0f};

        // Allocate GPU memory, if necessary

        x.ensure_gpu_up_to_date();
        y.ensure_gpu_allocated();

        // Perform pooling

        cudnn_check(cudnnPoolingForward(handle.get(), *pooling_desc, alpha, *x_tensor, x.gpu_memory(), beta, *y_tensor, y.gpu_memory()));

        y.validate_gpu();
        y.invalidate_cpu();
    }

    // Deep handling

    /*!
     * \brief Apply the functor on sub and store the result in m
     * \param sub The sub expression
     * \param m The storage matrix
     * \param c1 The first dimension pooling ratio
     * \param c2 The second dimension pooling ratio
     */
    template <typename A, typename M, cpp_enable_if((decay_traits<A>::dimensions > 4))>
    static void apply(const A& sub, M&& m, size_t c1, size_t c2, size_t s1, size_t s2, size_t p1, size_t p2) {
        for(size_t i = 0; i < etl::dim<0>(sub); ++i){
            apply(sub(i), m(i), c1, c2, s1, s2, p1, p2);
        }
    }
};

#else

//COVERAGE_EXCLUDE_BEGIN

//TODO

//COVERAGE_EXCLUDE_END

#endif

} //end of namespace cudnn

} //end of namespace impl

} //end of namespace etl