#!/bin/bash

# To suppress all warnings during installation
# make CXXFLAGS="-w" install

# Normal ChASE build

cmake .. \
-DCMAKE_INSTALL_PREFIX=/home/tartarughina/ChASE-def \
-DBUILD_WITH_EXAMPLES=ON \
-DNCCL_LIB_DIR=${NVIDIA_PATH}/comm_libs/nccl/lib \
-DNCCL_INCLUDE_DIR=${NVIDIA_PATH}/comm_libs/nccl/include \
-DCUDA_CUBLAS_LIBRARIES=${NVIDIA_PATH}/math_libs/lib64/libcublas.so \
-DCUDA_cusolver_LIBRARY=${NVIDIA_PATH}/math_libs/lib64/libcusolver.so \
-DCUDA_curand_LIBRARY=${NVIDIA_PATH}/math_libs/lib64/libcurand.so \
-DCHASE_OUTPUT=ON
# -DSCALAPACK_DIR=${NVIDIA_PATH}/comm_libs/mpi \


# Unified Memory ChASE build

cmake .. \
-DCMAKE_INSTALL_PREFIX=/home/tartarughina/ChASE-um \
-DBUILD_WITH_EXAMPLES=ON \
-DNCCL_LIB_DIR=${NVIDIA_PATH}/comm_libs/nccl/lib \
-DNCCL_INCLUDE_DIR=${NVIDIA_PATH}/comm_libs/nccl/include \
-DCUDA_CUBLAS_LIBRARIES=${NVIDIA_PATH}/math_libs/lib64/libcublas.so \
-DCUDA_cusolver_LIBRARY=${NVIDIA_PATH}/math_libs/lib64/libcusolver.so \
-DCUDA_curand_LIBRARY=${NVIDIA_PATH}/math_libs/lib64/libcurand.so \
-DENABLE_UM=ON

# Unified Memory Tuning ChASE build

cmake .. \
-DCMAKE_INSTALL_PREFIX=/home/tartarughina/ChASE-umt \
-DBUILD_WITH_EXAMPLES=ON \
-DNCCL_LIB_DIR=${NVIDIA_PATH}/comm_libs/nccl/lib \
-DNCCL_INCLUDE_DIR=${NVIDIA_PATH}/comm_libs/nccl/include \
-DCUDA_CUBLAS_LIBRARIES=${NVIDIA_PATH}/math_libs/lib64/libcublas.so \
-DCUDA_cusolver_LIBRARY=${NVIDIA_PATH}/math_libs/lib64/libcusolver.so \
-DCUDA_curand_LIBRARY=${NVIDIA_PATH}/math_libs/lib64/libcurand.so \
-DENABLE_UM=ON \
-DENABLE_TUNING=ON \
-DCHASE_OUTPUT=ON
# To ensure a correct build comment out or remove the lines setting cublas, cusolver and curand libraries
# To enable UM and tuning add the following lines to the cmake command
# -DENABLE_UM=ON
# -DENABLE_TUNING=ON
# For debugging purposes add the following line to the cmake command
# -DCHASE_OUTPUT=ON
