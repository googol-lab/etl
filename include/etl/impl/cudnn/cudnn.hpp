//=======================================================================
// Copyright (c) 2014-2017 Baptiste Wicht
// Distributed under the terms of the MIT License.
// (See accompanying file LICENSE or copy at
//  http://opensource.org/licenses/MIT)
//=======================================================================

/*!
 * \file
 * \brief Utility functions for cudnn
 */

#pragma once

#include "cudnn.h"

#define cudnn_check(call)                                                                                 \
    {                                                                                                     \
        cudnnStatus_t status = call;                                                                      \
        if (status != CUDNN_STATUS_SUCCESS) {                                                             \
            std::cerr << "CUDNN error: " << cudnnGetErrorString(status) << " from " << #call << std::endl \
                      << "from " << __FILE__ << ":" << __LINE__ << std::endl;                             \
        }                                                                                                 \
    }

namespace etl {

namespace impl {

namespace cudnn {

/*!
 * \brief RTTI helper to manage CUDNN handle
 */
struct cudnn_handle {
    cudnnHandle_t handle; ///< The raw cudnn handle

    /*!
     * \brief Construct the helper and create the handle directly
     */
    cudnn_handle(){
        cudnn_check(cudnnCreate(&handle));
    }

    /*!
     * \brief Construct the helper from the raw handle
     * \param handle The raw cudnn handle
     */
    explicit cudnn_handle(cudnnHandle_t handle) : handle(handle) {}

    cudnn_handle(const cudnn_handle& rhs) = delete;
    cudnn_handle& operator=(const cudnn_handle& rhs) = delete;

    cudnn_handle(cudnn_handle&& rhs) noexcept = default;
    cudnn_handle& operator=(cudnn_handle&& rhs) noexcept = default;

    /*!
     * \brief Get the cudnn handle
     * \return the raw cudnn handle
     */
    cudnnHandle_t get() {
        return handle;
    }

    /*!
     * \brief Destruct the helper and release the raw cudnn handle
     */
    ~cudnn_handle() {
        cudnn_check(cudnnDestroy(handle));
    }
};

#ifndef ETL_CUDNN_LOCAL_HANDLE

/*!
 * \brief Start cudnn and return a RTTI helper over a raw cudnn handle
 * \return RTTI helper over a raw cudnn handle
 */
inline cudnn_handle& start_cudnn() {
    static cudnn_handle handle;
    return handle;
}

#else

/*!
 * \brief Start cudnn and return a RTTI helper over a raw cudnn handle
 * \return RTTI helper over a raw cudnn handle
 */
inline cudnn_handle start_cudnn() {
    return {};
}

#endif

/*!
 * \brief Wrapper for CUDNN tensor.
 *
 * Needs to be specialized for the destructor.
 */
template<typename T>
struct cudnn_wrapper {
    T tensor; ///< The CUDNN tensor

    /*!
     * \brief Create a new cudnn_wrapper for the given tensor.
     */
    explicit cudnn_wrapper(T tensor) : tensor(tensor) {}

    /*!
     * \brief Move construct a cudnn_wrapper.
     *
     * This will remove the tensor from the RHS.
     *
     * \param rhs The right hand side cudnn_wrapper
     */
    cudnn_wrapper(cudnn_wrapper&& rhs){
        tensor = rhs.tensor;
        rhs.tensor = nullptr;
    }

    cudnn_wrapper(const cudnn_wrapper& rhs) = delete;
    cudnn_wrapper& operator=(const cudnn_wrapper& rhs) = delete;

    cudnn_wrapper& operator=(cudnn_wrapper&& rhs) = delete;

    /*!
     * \brief Convert back to the CUDNN tensor type.
     * \return The CUDNN tensor.
     */
    T operator*(){
        return tensor;
    }

    /*!
     * \brief Delete the cudnn_wrapper, releasing the tensor.
     */
    ~cudnn_wrapper();
};

/*!
 * \copydoc cudnn_wraper::~cudnn_wrapper()
 */
template<>
inline cudnn_wrapper<cudnnTensorDescriptor_t>::~cudnn_wrapper(){
    if(tensor){
        cudnn_check(cudnnDestroyTensorDescriptor(tensor));
    }
}

/*!
 * \copydoc cudnn_wraper::~cudnn_wrapper()
 */
template<>
inline cudnn_wrapper<cudnnFilterDescriptor_t>::~cudnn_wrapper(){
    if(tensor){
        cudnn_check(cudnnDestroyFilterDescriptor(tensor));
    }
}

/*!
 * \copydoc cudnn_wraper::~cudnn_wrapper()
 */
template<>
inline cudnn_wrapper<cudnnPoolingDescriptor_t>::~cudnn_wrapper(){
    if(tensor){
        cudnn_check(cudnnDestroyPoolingDescriptor(tensor));
    }
}

/*!
 * \brief Create a CUDNN tensor for the given input matrix
 * \param input The input matrix
 * \return a cudnn_wrapper around a created CUDNN tensor
 */
template<typename I, cpp_enable_if(decay_traits<I>::dimensions() == 2)>
cudnn_wrapper<cudnnTensorDescriptor_t> create_tensor(I&& input){
    using T = value_t<I>;

    auto data_type = std::is_same<std::remove_const_t<T>, float>::value ? CUDNN_DATA_FLOAT : CUDNN_DATA_DOUBLE;

    cudnnTensorDescriptor_t tensor;
    cudnn_check(cudnnCreateTensorDescriptor(&tensor));
    cudnn_check(cudnnSetTensor4dDescriptor(tensor, CUDNN_TENSOR_NCHW, data_type, 1, 1,
        etl::dim<0>(input), etl::dim<1>(input)));

    return cudnn_wrapper<cudnnTensorDescriptor_t>{tensor};
}

/*!
 * \brief Create a CUDNN tensor for the given input matrix
 * \param input The input matrix
 * \return a cudnn_wrapper around a created CUDNN tensor
 */
template<typename I, cpp_enable_if(decay_traits<I>::dimensions() == 3)>
cudnn_wrapper<cudnnTensorDescriptor_t> create_tensor(I&& input){
    using T = value_t<I>;

    auto data_type = std::is_same<std::remove_const_t<T>, float>::value ? CUDNN_DATA_FLOAT : CUDNN_DATA_DOUBLE;

    cudnnTensorDescriptor_t tensor;
    cudnn_check(cudnnCreateTensorDescriptor(&tensor));
    cudnn_check(cudnnSetTensor4dDescriptor(tensor, CUDNN_TENSOR_NCHW, data_type,
        1, etl::dim<0>(input), etl::dim<1>(input), etl::dim<2>(input)));

    return cudnn_wrapper<cudnnTensorDescriptor_t>{tensor};
}

/*!
 * \brief Create a CUDNN tensor for the given input matrix
 * \param input The input matrix
 * \return a cudnn_wrapper around a created CUDNN tensor
 */
template<typename I, cpp_enable_if(decay_traits<I>::dimensions() == 4)>
cudnn_wrapper<cudnnTensorDescriptor_t> create_tensor(I&& input){
    using T = value_t<I>;

    auto data_type = std::is_same<std::remove_const_t<T>, float>::value ? CUDNN_DATA_FLOAT : CUDNN_DATA_DOUBLE;

    cudnnTensorDescriptor_t tensor;
    cudnn_check(cudnnCreateTensorDescriptor(&tensor));
    cudnn_check(cudnnSetTensor4dDescriptor(tensor, CUDNN_TENSOR_NCHW, data_type,
        etl::dim<0>(input), etl::dim<1>(input), etl::dim<2>(input), etl::dim<3>(input)));

    return cudnn_wrapper<cudnnTensorDescriptor_t>{tensor};
}

/*!
 * \brief Create a CUDNN filter tensor for the given input matrix
 * \param input The input matrix
 * \return a cudnn_wrapper around a created CUDNN filter tensor
 */
template<typename I, cpp_enable_if(decay_traits<I>::dimensions() == 2)>
cudnn_wrapper<cudnnFilterDescriptor_t> create_filter(I&& kernel){
    using T = value_t<I>;

    auto data_type = std::is_same<std::remove_const_t<T>, float>::value ? CUDNN_DATA_FLOAT : CUDNN_DATA_DOUBLE;

    cudnnFilterDescriptor_t filter;
    cudnn_check(cudnnCreateFilterDescriptor(&filter));
    cudnn_check(cudnnSetFilter4dDescriptor(filter, data_type, CUDNN_TENSOR_NCHW,
        1, 1, etl::dim<0>(kernel), etl::dim<1>(kernel)));

    return cudnn_wrapper<cudnnFilterDescriptor_t>{filter};
}

/*!
 * \brief Create a CUDNN filter tensor for the given input matrix
 * \param input The input matrix
 * \return a cudnn_wrapper around a created CUDNN filter tensor
 */
template<typename I, cpp_enable_if(decay_traits<I>::dimensions() == 4)>
cudnn_wrapper<cudnnFilterDescriptor_t> create_filter(I&& kernel){
    using T = value_t<I>;

    auto data_type = std::is_same<std::remove_const_t<T>, float>::value ? CUDNN_DATA_FLOAT : CUDNN_DATA_DOUBLE;

    cudnnFilterDescriptor_t filter;
    cudnn_check(cudnnCreateFilterDescriptor(&filter));
    cudnn_check(cudnnSetFilter4dDescriptor(filter, data_type, CUDNN_TENSOR_NCHW,
        etl::dim<0>(kernel), etl::dim<1>(kernel), etl::dim<2>(kernel), etl::dim<3>(kernel)));

    return cudnn_wrapper<cudnnFilterDescriptor_t>{filter};
}

} //end of namespace cudnn

} //end of namespace impl

} //end of namespace etl
