#!/bin/bash

# To suppress all warnings during installation
# make CXXFLAGS="-w" install

# Normal ChASE build

cmake .. \
-DCMAKE_INSTALL_PREFIX=/home/tartarughina/ChASE-def \
-DCHASE_OUTPUT=ON \
-DBUILD_WITH_EXAMPLES=ON

# Unified Memory ChASE build

cmake .. \
-DCMAKE_INSTALL_PREFIX=/home/tartarughina/ChASE-um \
-DENABLE_UM=ON \
-DCHASE_OUTPUT=ON \
-DBUILD_WITH_EXAMPLES=ON

# Unified Memory Tuning ChASE build

cmake .. \
-DCMAKE_INSTALL_PREFIX=/home/tartarughina/ChASE-umt \
-DBUILD_WITH_EXAMPLES=ON \
-DENABLE_UM=ON \
-DENABLE_TUNING=ON

# To enable UM and tuning add the following lines to the cmake command
# -DENABLE_UM=ON
# -DENABLE_TUNING=ON
# For debugging purposes add the following line to the cmake command
# -DCHASE_OUTPUT=ON
