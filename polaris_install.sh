#!/bin/bash

# To suppress all warnings during installation
# make CXXFLAGS="-w" install

# Normal ChASE build

cmake .. \
-DCMAKE_INSTALL_PREFIX=/home/tartarughina/ChASE-def \
-DBUILD_WITH_EXAMPLES=ON

# Unified Memory ChASE build

cmake .. \
-DCMAKE_INSTALL_PREFIX=/home/tartarughina/ChASE-um \
-DENABLE_UM=ON \
-DBUILD_WITH_EXAMPLES=ON

# Unified Memory Tuning ChASE build

cmake .. \
-DCMAKE_INSTALL_PREFIX=/home/tartarughina/ChASE-umt \
-DBUILD_WITH_EXAMPLES=ON \
-DENABLE_UM=ON \
-DENABLE_TUNING=ON
# To ensure a correct build comment out or remove the lines setting cublas, cusolver and curand libraries
# To enable UM and tuning add the following lines to the cmake command
# -DENABLE_UM=ON
# -DENABLE_TUNING=ON
# For debugging purposes add the following line to the cmake command
# -DCHASE_OUTPUT=ON
