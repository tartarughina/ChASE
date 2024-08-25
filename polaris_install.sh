#!/bin/bash

cmake .. \
-DCMAKE_INSTALL_PREFIX=/home/tartarughina/ChASE-UM \
-DBUILD_WITH_EXAMPLES=ON \
-DNCCL_LIB_DIR=${NVIDIA_PATH}/comm_libs/nccl/lib \
-DNCCL_INCLUDE_DIR=${NVIDIA_PATH}/comm_libs/nccl/include \
-DCUDA_CUBLAS_LIBRARIES=${NVIDIA_PATH}/math_libs/lib64/libcublas.so \
-DCUDA_cusolver_LIBRARY=${NVIDIA_PATH}/math_libs/lib64/libcusolver.so \
-DCUDA_curand_LIBRARY=${NVIDIA_PATH}/math_libs/lib64/libcurand.so \
-DENABLE_UM=ON

# To ensure a correct build comment out or remove the lines setting cublas, cusolver and curand libraries
# To enable UM and tuning add the following lines to the cmake command
# -DENABLE_UM=ON
# -DENABLE_TUNING=ON
