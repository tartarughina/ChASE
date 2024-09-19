#!/bin/bash

# The compilation now requires NVHPC for the GPU version of Chase

# Normal ChASE build for the non Unified Memory version
# To

cmake .. \
-DCMAKE_INSTALL_PREFIX=/home/tartarughina/ChASE-def \
-DBUILD_WITH_EXAMPLES=ON

# Unified Memory ChASE build

cmake .. \
-DCMAKE_INSTALL_PREFIX=/home/tartarughina/ChASE-um \
-DBUILD_WITH_EXAMPLES=ON

# Unified Memory Tuning ChASE build

cmake .. \
-DCMAKE_INSTALL_PREFIX=/home/tartarughina/ChASE-umt \
-DBUILD_WITH_EXAMPLES=ON \
-DENABLE_TUNING=ON

# To ensure a correct build comment out or remove the lines setting cublas, cusolver and curand libraries
# To enable UM and tuning add the following lines to the cmake command
# -DENABLE_UM=ON
# -DENABLE_TUNING=ON
# For debugging purposes add the following line to the cmake command
# -DCHASE_OUTPUT=ON
