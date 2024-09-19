/* -*- Mode: C++; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
// This file is a part of ChASE.
// Copyright (c) 2015-2023, Simulation and Data Laboratory Quantum Materials,
//   Forschungszentrum Juelich GmbH, Germany. All rights reserved.
// License is 3-clause BSD:
// https://github.com/ChASE-library/ChASE

#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdio>
#include <memory>
#if defined(HAS_CUDA)
#include <cublas_v2.h>
#include <cuda.h>
#include <cuda_profiler_api.h>
#include <cuda_runtime.h>
#endif
#include "algorithm/types.hpp"

namespace chase
{
namespace mpi
{

template <class T>
class UnifiedMem
{
public:
    UnifiedMem() : size_(0), ptr_(nullptr), allocated_(false) {}

    UnifiedMem(std::size_t size) : size_(size), allocated_(true), type_("GPU")
    {
        int dev;
        cudaGetDevice(&dev);
        device_ = dev;

        cudaMallocManaged(&ptr_, size_ * sizeof(T));

#if defined(HAS_TUNING)
        cudaMemAdvise(ptr_, size_ * sizeof(T),
                      cudaMemAdviseSetPreferredLocation, device_);
        cudaMemAdvise(ptr_, size_ * sizeof(T), cudaMemAdviseSetAccessedBy,
                      device_);
        cudaMemAdvise(ptr_, size_ * sizeof(T), cudaMemAdviseSetAccessedBy,
                      cudaCpuDeviceId);
#endif

        cudaMemset(ptr_, 0, size_ * sizeof(T));
        cudaDeviceSynchronize();
    }

    UnifiedMem(T* ptr, std::size_t size)
        : size_(size), ptr_(ptr), allocated_(false), type_("GPU")
    {
    }

    ~UnifiedMem()
    {
        if (allocated_)
        {
            cudaFree(ptr_);
        }
    }

    int dev_id() { return device_; }

    T* ptr() { return ptr_; }

    bool isAlloc() { return allocated_; }

    std::string type() { return type_; }

private:
    int device_;
    std::size_t size_;
    T* ptr_;
    bool allocated_;
    std::string type_;
};

template <class T>
class Matrix
{
public:
    using ElementType = T;

    Matrix() : m_(0), n_(0), ld_(0), isHostAlloc_(false) {}
    // mode: 0: CPU, 1: traditional GPU, 2: CUDA-Aware
    Matrix(int mode, std::size_t m, std::size_t n)
        : m_(m), n_(n), ld_(m), mode_(mode)
    {
        switch (mode)
        {
            default: // Unified Memory
                printf("Unified Memory matrix creation\n");
                Device_ = std::make_shared<UnifiedMem<T>>(m * n);
                Host_ = Device_;
                isHostAlloc_ = false;
                isDeviceAlloc_ = true;
                break;
        }
    }

    Matrix(int mode, std::size_t m, std::size_t n, T* ptr, std::size_t ld)
        : m_(m), n_(n), ld_(ld), mode_(mode)
    {
        switch (mode)
        {
            default:
                printf("Unified Memory matrix creation\n");
                Host_ = std::make_shared<UnifiedMem<T>>(ptr, ld * n);
                Device_ = Host_;
                isHostAlloc_ = true;
                isDeviceAlloc_ = true;
                break;
        }
    }

    bool isHostAlloc() { return false; }
    T* host() { return Host_.get()->ptr(); }

    T* ptr()
    {
        T* ptr;
        if (isHostAlloc_)
        {
            ptr = Host_.get()->ptr();
        }
#if defined(HAS_CUDA)
        else if (isDeviceAlloc_)
        {
            ptr = Device_.get()->ptr();
        }
#endif
        return ptr;
    }

    void swap(Matrix<T>& swapping_obj)
    {
        std::swap(Host_, swapping_obj.Host_);
        cudaDeviceSynchronize();
    }

#if defined(HAS_CUDA)
    T* device() { return Device_.get()->ptr(); }
#endif
    std::size_t ld() { return ld_; }

    std::size_t h_ld() { return ld_; }
#if defined(HAS_CUDA)
    std::size_t d_ld() { return m_; }
#endif
// cublasSet and cublasGet are just wrappers to memcpy therefore for UM all i
// need is to prefetch stuff from/to the device when tuning is enabled or ignore
// when not enabled
#if defined(HAS_CUDA)
    void H2D()
    {
#if defined(HAS_TUNING)
        cudaMemPrefetchAsync(this->device(), ld_ * n_ * sizeof(T),
                             Device_.get()->dev_id(), 0);
        cudaDeviceSynchronize();
#endif
    }

    void H2D(std::size_t nrows, std::size_t ncols, std::size_t offset = 0)
    {
#if defined(HAS_TUNING)
        // Prefetch the specific part of the matrix to the GPU
        cudaMemPrefetchAsync(this->device() + offset * this->d_ld(),
                             nrows * ncols * sizeof(T), Device_.get()->dev_id(),
                             0);
        cudaDeviceSynchronize();

#endif
    }

    void D2H()
    {
#if defined(HAS_TUNING)
        // Prefetch the specific part of the matrix to the GPU
        cudaMemPrefetchAsync(this->device(), ld_ * n_ * sizeof(T),
                             cudaCpuDeviceId, 0);
        cudaDeviceSynchronize();

#endif
    }

    void D2H(std::size_t nrows, std::size_t ncols, std::size_t offset = 0)
    {
#if defined(HAS_TUNING)
        // Prefetch the specific part of the matrix to the GPU
        cudaMemPrefetchAsync(this->device() + offset * this->d_ld(),
                             nrows * ncols * sizeof(T), cudaCpuDeviceId, 0);
        cudaDeviceSynchronize();

#endif
    }
#endif

    void sync2Ptr(std::size_t nrows, std::size_t ncols, std::size_t offset = 0)
    {
#if defined(HAS_TUNING)
        // Prefetch the specific part of the matrix to the GPU
        cudaMemPrefetchAsync(this->device() + offset * this->d_ld(),
                             nrows * ncols * sizeof(T), cudaCpuDeviceId, 0);
        cudaDeviceSynchronize();

#endif
    }

    void sync2Ptr()
    {
#if defined(HAS_TUNING)
        // Prefetch the specific part of the matrix to the GPU
        cudaMemPrefetchAsync(this->device(), ld_ * n_ * sizeof(T),
                             cudaCpuDeviceId, 0);
        cudaDeviceSynchronize();

#endif
    }

    void syncFromPtr(std::size_t nrows, std::size_t ncols,
                     std::size_t offset = 0)
    {
#if defined(HAS_TUNING)
        // Prefetch the specific part of the matrix to the GPU
        cudaMemPrefetchAsync(this->device() + offset * this->d_ld(),
                             nrows * ncols * sizeof(T), Device_.get()->dev_id(),
                             0);
        cudaDeviceSynchronize();
#endif
    }

    void syncFromPtr()
    {
#if defined(HAS_TUNING)
        cudaMemPrefetchAsync(this->device(), ld_ * n_ * sizeof(T),
                             Device_.get()->dev_id(), 0);
        cudaDeviceSynchronize();

#endif
    }

private:
    std::size_t m_;
    std::size_t n_;
    std::size_t ld_;
    std::shared_ptr<UnifiedMem<T>> Host_;
    std::shared_ptr<UnifiedMem<T>> Device_;
    bool isHostAlloc_ = false;
    bool isDeviceAlloc_ = false;
    bool mode_;
};
/*
 *  Utility class for Buffers
 */
//! @brief A class to setup the buffers of matrices and vectors which will be
//! used by ChaseMpi.
/*!
  This class provides three constructors:
  - Allocating the buffers for ChaseMpi without MPI support.
  - Allocating the buffers for ChaseMpi with MPI support.
  -  Allocating the buffers for ChaseMpi with MPI support in which the buffer
  to store the matrix to be diagonalised is externally allocated and provided by
  users.
  @tparam T: the scalar type used for the application. ChASE is templated
    for real and complex scalar with both Single Precision and Double Precision,
    thus `T` can be one of `float`, `double`, `std::complex<float>` and
    `std::complex<double>`.
*/
template <class T>
class ChaseMpiMatrices
{
public:
    //! A constructor of ChaseMpiMatrices for **Non-MPI case** which allocates
    //! everything required.
    /*!
      The **private members** of this class are initialized by the parameters of
      this constructor.
      - For `H__` and `H_`, they are of size `ldh * N`.
      - For `V1__`, `V1_`, `V2__` and `V1_`, they are of size `N * max_block`.
      - For `ritzv__`, `ritzv_`, `resid__` and `resid_`, they are of size
      `max_block`.
      @param N: size of the square matrix defining the eigenproblem.
      @param max_block: Maximum column number of matrix `V1_` and `V2_`. It
      equals to `nev_ + nex_`.
      @param H: a pointer to the buffer `H_`.
      @param ldh: the leading dimension of `H_`.
      @param V1: a pointer to the buffer `V1_`.
      @param ritz: a pointer to the buffer `ritz_`.
      @param V2: a pointer to the buffer `V2_`.
      @param resid: a pointer to the buffer `resid_`.
    */

    ChaseMpiMatrices() {}

    ChaseMpiMatrices(int mode, std::size_t N, std::size_t max_block, T* H,
                     std::size_t ldh, T* V1, Base<T>* ritzv, T* V2 = nullptr,
                     Base<T>* resid = nullptr)
        : mode_(mode), ldh_(ldh)
    {
        int isGPU = 0;
        int onlyGPU = 0;

        H___ = std::make_unique<Matrix<T>>(isGPU, N, N, H, ldh);
        C___ = std::make_unique<Matrix<T>>(isGPU, N, max_block, V1, N);
        B___ = std::make_unique<Matrix<T>>(onlyGPU, N, max_block);
        A___ = std::make_unique<Matrix<T>>(onlyGPU, max_block, max_block);
        Ritzv___ = std::make_unique<Matrix<Base<T>>>(isGPU, 1, max_block, ritzv,
                                                     max_block);
        Resid___ = std::make_unique<Matrix<Base<T>>>(isGPU, 1, max_block);
    }

    //! A constructor of ChaseMpiMatrices for **MPI case** which allocates
    //! everything necessary except `H_`.
    /*!
      The **private members** of this class are initialized by the parameters of
      this constructor.
      - For `V1__` and `V1_`, they are of size `m_ * max_block`.
      - For `V2__` and `V2_`, they are of size `n_ * max_block`.
      - For `ritzv__`, `ritzv_`, `resid__` and `resid_`, they are of size
      `max_block`.
      - `H_` is allocated externally based the users, it is of size `ldh_ * n_`
      with `ldh_>=m_`.
      - `m` and `n` can be obtained through ChaseMpiProperties::get_m() and
      ChaseMpiProperties::get_n(), respecitvely.
      @param comm: the working MPI communicator of ChASE.
      @param N: size of the square matrix defining the eigenproblem.
      @param m: row number of `H_`.`m` can be obtained through
       ChaseMpiProperties::get_m()
      @param n: column number of `H_`.`n` can be obtained through
       ChaseMpiProperties::get_n()
      @param max_block: Maximum column number of matrix `V1_` and `V2_`. It
      equals to `nev_ + nex_`.
      @param H: the pointer to the user-provided buffer of matrix to be
      diagonalised.
      @param ldh: The leading dimension of local part of Symmetric/Hermtian
      matrix on each MPI proc.
      @param V1: a pointer to the buffer `V1_`.
      @param ritz: a pointer to the buffer `ritz_`.
      @param V2: a pointer to the buffer `V2_`.
      @param resid: a pointer to the buffer `resid_`.
    */
    ChaseMpiMatrices(int mode, MPI_Comm comm, std::size_t N, std::size_t m,
                     std::size_t n, std::size_t max_block, T* H,
                     std::size_t ldh, T* V1, Base<T>* ritzv)
        : mode_(mode), ldh_(ldh)
    {
        int isGPU;
        int isCUDA_Aware;

        H___ = std::make_unique<Matrix<T>>(isGPU, m, n, H, ldh);
        C___ = std::make_unique<Matrix<T>>(isCUDA_Aware, m, max_block, V1, m);
        C2___ = std::make_unique<Matrix<T>>(isCUDA_Aware, m, max_block);
        B___ = std::make_unique<Matrix<T>>(isCUDA_Aware, n, max_block);
        B2___ = std::make_unique<Matrix<T>>(isCUDA_Aware, n, max_block);
        A___ = std::make_unique<Matrix<T>>(isCUDA_Aware, max_block, max_block);
        Ritzv___ = std::make_unique<Matrix<Base<T>>>(isGPU, 1, max_block, ritzv,
                                                     max_block);
        Resid___ = std::make_unique<Matrix<Base<T>>>(isGPU, 1, max_block);
        vv___ = std::make_unique<Matrix<T>>(isGPU, m, 1);
    }

    int get_Mode() { return mode_; }
    //! Return leading dimension of local part of Symmetric/Hermtian matrix on
    //! each MPI proc.
    /*! \return `ldh_`, a private member of this class.
     */
    std::size_t get_ldh() { return ldh_; }

    Matrix<T> H() { return *H___.get(); }
    Matrix<T> C() { return *C___.get(); }
    Matrix<T> C2() { return *C2___.get(); }
    Matrix<T> A() { return *A___.get(); }
    Matrix<T> B() { return *B___.get(); }
    Matrix<T> B2() { return *B2___.get(); }
    Matrix<Base<T>> Resid() { return *Resid___.get(); }
    Matrix<Base<T>> Ritzv() { return *Ritzv___.get(); }
    Matrix<T> vv() { return *vv___.get(); }
    T* C_comm()
    {
        T* C;
        switch (mode_)
        {
            default:
                C = this->C().host();
                break;
        }

        return C;
    }

    T* C2_comm()
    {
        T* C2;
        switch (mode_)
        {
            default:
                C2 = this->C2().host();
                break;
        }
        return C2;
    }

    T* B_comm()
    {
        T* B;
        switch (mode_)
        {
            default:
                B = this->B().host();
                break;
        }
        return B;
    }

    T* B2_comm()
    {
        T* B2;
        switch (mode_)
        {
            default:
                B2 = this->B2().host();
                break;
        }
        return B2;
    }

    T* A_comm()
    {
        T* a;
        switch (mode_)
        {
            default:
                a = this->A().host();
                break;
        }
        return a;
    }

    Base<T>* Resid_comm()
    {
        Base<T>* rsd;
        switch (mode_)
        {
            default:
                rsd = this->Resid().host();
                break;
        }
        return rsd;
    }

    T* vv_comm()
    {
        T* v;
        switch (mode_)
        {
            default:
                v = this->vv().host();
                break;
        }
        return v;
    }

private:
    std::size_t ldh_;
    int mode_;

    std::unique_ptr<Matrix<T>> H___;
    std::unique_ptr<Matrix<T>> C___;
    std::unique_ptr<Matrix<T>> C2___;
    std::unique_ptr<Matrix<T>> B___;
    std::unique_ptr<Matrix<T>> B2___;
    std::unique_ptr<Matrix<T>> A___;
    std::unique_ptr<Matrix<Base<T>>> Resid___;
    std::unique_ptr<Matrix<Base<T>>> Ritzv___;
    std::unique_ptr<Matrix<T>> vv___;
};
} // namespace mpi
} // namespace chase
