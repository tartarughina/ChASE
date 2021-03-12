/* -*- Mode: C++; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
// This file is a part of ChASE.
// Copyright (c) 2015-2018, Simulation Laboratory Quantum Materials,
//   Forschungszentrum Juelich GmbH, Germany
// and
// Copyright (c) 2016-2018, Aachen Institute for Advanced Study in Computational
//   Engineering Science, RWTH Aachen University, Germany All rights reserved.
// License is 3-clause BSD:
// https://github.com/SimLabQuantumMaterials/ChASE/

#pragma once

#include <cstring>
#include <memory>

#include "ChASE-MPI/blas_templates.hpp"
#include "ChASE-MPI/chase_mpi_matrices.hpp"

namespace chase {
namespace mpi {

// A very simple implementation of MatrixFreeInterface
// We duplicate the two vector sets from ChASE_Blas and copy
// into the duplicates before each GEMM call.

template <class T>
class ChaseMpiDLABlaslapackSeq : public ChaseMpiDLAInterface<T> {
 public:
  explicit ChaseMpiDLABlaslapackSeq(ChaseMpiMatrices<T>& matrices, std::size_t n,
                               std::size_t maxBlock)
      : N_(n),
        H_(matrices.get_H()),
        V1_(new T[N_ * maxBlock]),
        V2_(new T[N_ * maxBlock]) {}

  ChaseMpiDLABlaslapackSeq() = delete;
  ChaseMpiDLABlaslapackSeq(ChaseMpiDLABlaslapackSeq const& rhs) = delete;

  ~ChaseMpiDLABlaslapackSeq() {}

  void preApplication(T* V, std::size_t const locked,
                      std::size_t const block) override {
    locked_ = locked;
    std::memcpy(get_V1(), V + locked * N_, N_ * block * sizeof(T));
  }

  void preApplication(T* V1, T* V2, std::size_t const locked,
                      std::size_t const block) override {
    std::memcpy(get_V2(), V2 + locked * N_, N_ * block * sizeof(T));
    this->preApplication(V1, locked, block);
  }

  void apply(T const alpha, T const beta, const std::size_t offset,
             const std::size_t block) override {
    t_gemm(CblasColMajor, CblasNoTrans, CblasNoTrans,  //
           N_, block, N_,                              //
           &alpha,                                     // V2 <-
           H_, N_,                                     //      alpha * H*V1
           get_V1() + offset * N_, N_,                 //      + beta * V2
           &beta,                                      //
           get_V2() + offset * N_, N_);
    std::swap(V1_, V2_);
  }

  bool postApplication(T* V, std::size_t const block) override {
    std::memcpy(V + locked_ * N_, get_V1(), N_ * block * sizeof(T));
    return false;
  }

  void shiftMatrix(T const c, bool isunshift = false) override {
    for (std::size_t i = 0; i < N_; ++i) {
      H_[i + i * N_] += c;
    }
  }

  void applyVec(T* B, T* C) override {
    T const alpha = T(1.0);
    T const beta = T(0.0);

    t_gemm(CblasColMajor, CblasNoTrans, CblasNoTrans,  //
           N_, 1, N_,                                  //
           &alpha,                                     // C <-
           H_, N_,                                     //     1.0 * H*B
           B, N_,                                      //     + 0.0 * C
           &beta,                                      //
           C, N_);
  }

  void get_off(std::size_t* xoff, std::size_t* yoff, std::size_t* xlen,
               std::size_t* ylen) const override {
    *xoff = 0;
    *yoff = 0;
    *xlen = N_;
    *ylen = N_;
  }

  T* get_H() const  override { return H_; }

  T* get_V1() const { return V1_.get(); }
  T* get_V2() const { return V2_.get(); }
  std::size_t get_mblocks() const override {return 1;}
  std::size_t get_nblocks() const override {return 1;}
  std::size_t get_n() const override {return N_;}
  std::size_t get_m() const override {return N_;}
  int *get_coord() const override {
	  int *coord = new int[2];
	  coord[0] = coord[1] = 0;
	  return coord;
  }
  void get_offs_lens(std::size_t* &r_offs, std::size_t* &r_lens, std::size_t* &r_offs_l,
                  std::size_t* &c_offs, std::size_t* &c_lens, std::size_t* &c_offs_l) const override{

	  std::size_t r_offs_[1] = {0};
          std::size_t r_lens_[1]; r_lens_[0] = N_;
	  std::size_t r_offs_l_[1] = {0};
          std::size_t c_offs_[1] = {0};
          std::size_t c_lens_[1]; r_lens_[0] = N_;
          std::size_t c_offs_l_[1] = {0};

	  r_offs = r_offs_;
	  r_lens = r_lens_;
	  r_offs_l = r_offs_l_;
          c_offs = c_offs_;
          c_lens = c_lens_;
          c_offs_l = c_offs_l_;
  }

  void Start() override {}

  Base<T> lange(char norm, std::size_t m, std::size_t n, T* A, std::size_t lda) override {
      return t_lange(norm, m, n, A, lda);
  }

  void gegqr(std::size_t N, std::size_t nevex, T * approxV, std::size_t LDA) override {
      auto tau = std::unique_ptr<T[]> {
          new T[ nevex ]
      };
      t_geqrf(LAPACK_COL_MAJOR, N, nevex, approxV, LDA, tau.get());
      t_gqr(LAPACK_COL_MAJOR, N, nevex, nevex, approxV, LDA, tau.get());
  }

  void axpy(std::size_t N, T * alpha, T * x, std::size_t incx, T *y, std::size_t incy) override {
      t_axpy(N, alpha, x, incx, y, incy);
  }

  void scal(std::size_t N, T *a, T *x, std::size_t incx) override {
      t_scal(N, a, x, incx);
  }

  Base<T> nrm2(std::size_t n, T *x, std::size_t incx) override {
      return t_nrm2(n, x, incx);
  }

  T dot(std::size_t n, T* x, std::size_t incx, T* y, std::size_t incy) override {
      return t_dot(n, x, incx, y, incy);
  }

  void gemm_small(CBLAS_LAYOUT Layout, CBLAS_TRANSPOSE transa,
                         CBLAS_TRANSPOSE transb, std::size_t m,
                         std::size_t n, std::size_t k, T* alpha,
                         T* a, std::size_t lda, T* b,
                         std::size_t ldb, T* beta, T* c, std::size_t ldc) override 
  {
      t_gemm(Layout, transa, transb, m, n, k, alpha, a, lda, b, ldb, beta, c, ldc);
  }

  void gemm_large(CBLAS_LAYOUT Layout, CBLAS_TRANSPOSE transa,
                         CBLAS_TRANSPOSE transb, std::size_t m,
                         std::size_t n, std::size_t k, T* alpha,
                         T* a, std::size_t lda, T* b,
                         std::size_t ldb, T* beta, T* c, std::size_t ldc) override 
  {
      t_gemm(Layout, transa, transb, m, n, k, alpha, a, lda, b, ldb, beta, c, ldc);
  }

  std::size_t stemr(int matrix_layout, char jobz, char range, std::size_t n,
                    double* d, double* e, double vl, double vu, std::size_t il, std::size_t iu,
                    int* m, double* w, double* z, std::size_t ldz, std::size_t nzc,
                    int* isuppz, lapack_logical* tryrac) override {
      return t_stemr<double>(matrix_layout, jobz, range, n, d, e, vl, vu, il, iu, m, w, z, ldz, nzc, isuppz, tryrac);
  }

  std::size_t stemr(int matrix_layout, char jobz, char range, std::size_t n,
                    float* d, float* e, float vl, float vu, std::size_t il, std::size_t iu,
                    int* m, float* w, float* z, std::size_t ldz, std::size_t nzc,
                    int* isuppz, lapack_logical* tryrac) override {
      return t_stemr<float>(matrix_layout, jobz, range, n, d, e, vl, vu, il, iu, m, w, z, ldz, nzc, isuppz, tryrac);
  }

  void RR_kernel(std::size_t N, std::size_t block, T *approxV, std::size_t locked, T *workspace, T One, T Zero, Base<T> *ritzv) override {
      T *A = new T[block * block];

      // A <- W' * V
      t_gemm(CblasColMajor, CblasConjTrans, CblasNoTrans,
             block, block, N,
             &One,
             approxV + locked * N, N,
             workspace + locked * N, N,
             &Zero,
             A, block
      );

      t_heevd(LAPACK_COL_MAJOR, 'V', 'L', block, A, block, ritzv);

      t_gemm(CblasColMajor, CblasNoTrans, CblasNoTrans,
           N, block, block,
           &One,
           approxV + locked * N, N,
           A, block,
           &Zero,
           workspace + locked * N, N
      );

      delete[] A;
  }

 private:
  std::size_t N_;
  std::size_t locked_;
  T* H_;
  std::unique_ptr<T> V1_;
  std::unique_ptr<T> V2_;
};

template <typename T>
struct is_skewed_matrixfree<ChaseMpiDLABlaslapackSeq<T>> {
  static const bool value = false;
};

}  // namespace mpi
}  // namespace chase
