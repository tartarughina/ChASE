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
class CpuMem
{
public:
    CpuMem() : size_(0), ptr_(nullptr), allocated_(false) {}
    CpuMem(std::size_t size, bool useGPU = false)
        : size_(size), allocated_(true), useGPU_(useGPU), type_("CPU")
    {
        if (!useGPU)
        {
            ptr_ = std::allocator<T>().allocate(size_);
        }
#if defined(HAS_CUDA)
        else
        {
            // Add an option here to use cudaMallocManaged based on the option
            // for Unified Memory
            cudaMallocHost(&ptr_, size_ * sizeof(T));
        }
#endif
        std::fill_n(ptr_, size_, T(0.0));
    }

    // The option to use a pre allocated pointer is avaialable...
    // Maybe this is the way to use Unified Memory
    CpuMem(T* ptr, std::size_t size)
        : size_(size), ptr_(ptr), allocated_(false), type_("CPU")
    {
    }

    ~CpuMem()
    {
        if (allocated_)
        {
            if (!useGPU_)
            {
                std::allocator<T>().deallocate(ptr_, size_);
            }
#if defined(HAS_CUDA)
            else
            {
                // Due to this element I don't think the option to pass a
                // Unified Memory pointer is the right one as the class will
                // try to free memory using the wrong method
                cudaFreeHost(ptr_);
            }
#endif
        }
    }

    T* ptr() { return ptr_; }

    bool isAlloc() { return allocated_; }

    std::string type() { return type_; }

private:
    std::size_t size_;
    T* ptr_;
    bool allocated_;
    std::string type_;
    bool useGPU_;
};

/**
 * An option is to create another type of memory which is Unified Memory,
 * removing in this way the complexity of changing the previous one and the
 * upcoming ones
 */

#if defined(HAS_CUDA)
template <class T>
class GpuMem
{
public:
    GpuMem() : size_(0), ptr_(nullptr), allocated_(false) {}

    GpuMem(std::size_t size) : size_(size), allocated_(true), type_("GPU")
    {
        cudaMalloc(&ptr_, size_ * sizeof(T));
        cudaMemset(ptr_, 0, size_ * sizeof(T));
    }

    GpuMem(T* ptr, std::size_t size)
        : size_(size), ptr_(ptr), allocated_(false), type_("GPU")
    {
    }

    ~GpuMem()
    {
        if (allocated_)
        {
            cudaFree(ptr_);
        }
    }

    T* ptr() { return ptr_; }

    bool isAlloc() { return allocated_; }

    std::string type() { return type_; }

private:
    std::size_t size_;
    T* ptr_;
    bool allocated_;
    std::string type_;
};
#if defined(HAS_UM)
template <class T>
class UnifiedMem
{
public:
    UnifiedMem() : size_(0), ptr_(nullptr), allocated_(false) {}

    UnifiedMem(std::size_t size) : size_(size), allocated_(true), type_("CPU")
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
    }

    UnifiedMem(T* ptr, std::size_t size)
        : size_(size), ptr_(ptr), allocated_(false), type_("CPU")
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
#endif
#endif

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
#if defined(HAS_UM)
            case 0:
            case 1:
            case 2:
            case 3: // Unified Memory
                Host_ = std::make_shared<UnifiedMem<T>>(m * n);
                Device_ = Host_;
                isHostAlloc_ = false;
                isDeviceAlloc_ = true;
                break;
#else
            case 0: // CPU
                Host_ = std::make_shared<CpuMem<T>>(m * n);
                isHostAlloc_ = true;
                isDeviceAlloc_ = false;
                break;
#if defined(HAS_CUDA)
            case 1: // Traditional GPU
                Host_ = std::make_shared<CpuMem<T>>(m * n, true);
                Device_ = std::make_shared<GpuMem<T>>(m * n);
                isHostAlloc_ = true;
                isDeviceAlloc_ = true;
                break;
            case 2: // CUDA-Aware
                Device_ = std::make_shared<GpuMem<T>>(m * n);
                isHostAlloc_ = false;
                isDeviceAlloc_ = true;
                break;
#endif
#endif
        }
    }

    Matrix(int mode, std::size_t m, std::size_t n, T* ptr, std::size_t ld)
        : m_(m), n_(n), ld_(ld), mode_(mode)
    {
        switch (mode)
        {
#if defined(HAS_UM)
            case 0:
            case 1:
            case 2:
            case 3:
                Host_ = std::make_shared<UnifiedMem<T>>(ptr, ld * n);
                Device_ = Host_;
                // Match m with ld as m may differ from ld
                m_ = ld;
                isHostAlloc_ = true;
                isDeviceAlloc_ = true;
                break;
#else
            case 0:
                Host_ = std::make_shared<CpuMem<T>>(ptr, ld * n);
                isHostAlloc_ = true;
                isDeviceAlloc_ = false;
                break;
#if defined(HAS_CUDA)
            case 1:
                Host_ = std::make_shared<CpuMem<T>>(ptr, ld * n);
                Device_ = std::make_shared<GpuMem<T>>(m * n);
                isHostAlloc_ = true;
                isDeviceAlloc_ = true;
                break;
            case 2:
                Host_ = std::make_shared<CpuMem<T>>(ptr, ld * n);
                Device_ = std::make_shared<GpuMem<T>>(m * n);
                isHostAlloc_ = true;
                isDeviceAlloc_ = true;
                break;
#endif
#endif
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

#if defined(HAS_UM)
    int dev_id() { return Host_.get()->dev_id(); }
#endif

    void swap(Matrix<T>& swapping_obj)
    {
        std::swap(Host_, swapping_obj.Host_);

#if defined(HAS_CUDA)
#if defined(HAS_UM)
        // Update UM with the new Host pointer, do the same with the swapping
        // object to maintain consistency
        // Using std::swap also on device would not work
        Device_ = Host_;
        swapping_obj.Device_ = swapping_obj.Host_;
#else
        std::swap(Device_, swapping_obj.Device_);
#endif
#endif
    }

#if defined(HAS_CUDA)
    T* device() { return Device_.get()->ptr(); }
#endif
    std::size_t ld() { return ld_; }

    std::size_t h_ld() { return ld_; }
#if defined(HAS_CUDA)
    std::size_t d_ld() { return m_; }
#endif
    // cublasSet and cublasGet are just wrappers to memcpy therefore for UM all
    // i need is to prefetch stuff from/to the device when tuning is enabled or
    // ignore when not enabled
    void sync2Ptr(std::size_t nrows, std::size_t ncols, std::size_t offset = 0)
    {
#if defined(HAS_CUDA)
#if defined(HAS_UM)
#if defined(HAS_TUNING)
        cudaMemPrefetchAsync(this->device() + offset * this->d_ld(),
                             nrows * ncols * sizeof(T), cudaCpuDeviceId, 0);

#endif
        cudaDeviceSynchronize();
#else
        cublasGetMatrix(nrows, ncols, sizeof(T),
                        this->device() + offset * this->d_ld(), this->d_ld(),
                        this->host() + offset * this->h_ld(), this->h_ld());
#endif
#endif
    }

    void sync2Ptr()
    {
#if defined(HAS_CUDA)
#if defined(HAS_UM)
#if defined(HAS_TUNING)
        cudaMemPrefetchAsync(this->device(), m_ * n_ * sizeof(T),
                             cudaCpuDeviceId, 0);

#endif
        cudaDeviceSynchronize();
#else
        cublasGetMatrix(m_, n_, sizeof(T), this->device(), this->d_ld(),
                        this->host(), this->h_ld());
#endif
#endif
    }

    void syncFromPtr(std::size_t nrows, std::size_t ncols,
                     std::size_t offset = 0)
    {
#if defined(HAS_CUDA)
#if defined(HAS_UM)
#if defined(HAS_TUNING)
        cudaMemPrefetchAsync(this->device() + offset * this->d_ld(),
                             nrows * ncols * sizeof(T), Device_.get()->dev_id(),
                             0);
#endif
#else
        cublasSetMatrix(nrows, ncols, sizeof(T),
                        this->host() + offset * this->h_ld(), this->h_ld(),
                        this->device() + offset * this->d_ld(), this->d_ld());
#endif
#endif
    }

    void syncFromPtr()
    {
#if defined(HAS_CUDA)
#if defined(HAS_UM)
#if defined(HAS_TUNING)
        cudaMemPrefetchAsync(this->device(), ld_ * n_ * sizeof(T),
                             Device_.get()->dev_id(), 0);
#endif
#else
        cublasSetMatrix(m_, n_, sizeof(T), this->host(), this->h_ld(),
                        this->device(), this->d_ld());
#endif
#endif
    }

#if defined(HAS_CUDA)
    void H2D() { this->syncFromPtr(); }

    void H2D(std::size_t nrows, std::size_t ncols, std::size_t offset = 0)
    {
        this->syncFromPtr(nrows, ncols, offset);
    }

    void D2H() { this->sync2Ptr(); }

    void D2H(std::size_t nrows, std::size_t ncols, std::size_t offset = 0)
    {
        this->sync2Ptr(nrows, ncols, offset);
    }
#endif

private:
    std::size_t m_;
    std::size_t n_;
    std::size_t ld_;
#if defined(HAS_UM)
    std::shared_ptr<UnifiedMem<T>> Host_;
    std::shared_ptr<UnifiedMem<T>> Device_;
#else
    std::shared_ptr<CpuMem<T>> Host_;
#if defined(HAS_CUDA)
    std::shared_ptr<GpuMem<T>> Device_;
#endif
#endif
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
        if (mode == 1)
        {
            isGPU = 1;
        }
        int onlyGPU = 0;
        if (isGPU)
        {
            onlyGPU = 2;
        }

#if defined(HAS_UM)
        // mode 3 is for Unified Memory
        if (mode == 3)
        {
            isGPU = 3;
            onlyGPU = 3;
        }
#endif
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
        if (mode == 0)
        {
            isGPU = 0;
            isCUDA_Aware = 0;
        }
        else if (mode == 1)
        {
            isGPU = 1;
            isCUDA_Aware = 1;
        }
        else
        {
            isGPU = 1;
            isCUDA_Aware = 2;
        }

#if defined(HAS_UM)
        // mode 3 is for Unified Memory
        if (mode == 3)
        {
            isGPU = 3;
            isCUDA_Aware = 3;
        }
#endif
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
            case 0:
                C = this->C().host();
                break;
#if defined(HAS_CUDA)
            case 1:
                C = this->C().host();
                break;
            case 2:
                C = this->C().device();
                break;
#if defined(HAS_UM)
            case 3:
                C = this->C().device();
                break;
#endif
#endif
        }
        return C;
    }

    T* C2_comm()
    {
        T* C2;
        switch (mode_)
        {
            case 0:
                C2 = this->C2().host();
                break;
#if defined(HAS_CUDA)
            case 1:
                C2 = this->C2().host();
                break;
            case 2:
                C2 = this->C2().device();
                break;
#if defined(HAS_UM)
            case 3:
                C2 = this->C2().device();
                break;
#endif
#endif
        }
        return C2;
    }

    T* B_comm()
    {
        T* B;
        switch (mode_)
        {
            case 0:
                B = this->B().host();
                break;
#if defined(HAS_CUDA)
            case 1:
                B = this->B().host();
                break;
            case 2:
                B = this->B().device();
                break;
#if defined(HAS_UM)
            case 3:
                B = this->B().device();
                break;
#endif
#endif
        }
        return B;
    }

    T* B2_comm()
    {
        T* B2;
        switch (mode_)
        {
            case 0:
                B2 = this->B2().host();
                break;
#if defined(HAS_CUDA)
            case 1:
                B2 = this->B2().host();
                break;
            case 2:
                B2 = this->B2().device();
                break;
#if defined(HAS_UM)
            case 3:
                B2 = this->B2().device();
                break;
#endif
#endif
        }
        return B2;
    }

    T* A_comm()
    {
        T* a;
        switch (mode_)
        {
            case 0:
                a = this->A().host();
                break;
#if defined(HAS_CUDA)
            case 1:
                a = this->A().host();
                break;
            case 2:
                a = this->A().device();
                break;
#if defined(HAS_UM)
            case 3:
                a = this->A().device();
                break;
#endif
#endif
        }
        return a;
    }

    Base<T>* Resid_comm()
    {
        Base<T>* rsd;
        switch (mode_)
        {
            case 0:
                rsd = this->Resid().host();
                break;
#if defined(HAS_CUDA)
            case 1:
                rsd = this->Resid().host();
                break;
            case 2:
                rsd = this->Resid().device();
                break;
#if defined(HAS_UM)
            case 3:
                rsd = this->Resid().device();
                break;
#endif
#endif
        }
        return rsd;
    }

    T* vv_comm()
    {
        T* v;
        switch (mode_)
        {
            case 0:
                v = this->vv().host();
                break;
#if defined(HAS_CUDA)
            case 1:
                v = this->vv().host();
                break;
            case 2:
                v = this->vv().device();
                break;
#if defined(HAS_UM)
            case 3:
                v = this->vv().device();
                break;
#endif
#endif
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
