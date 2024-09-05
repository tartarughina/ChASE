# -*- Mode: cmake -*-
cmake_minimum_required( VERSION 3.8 )

project( ChASE LANGUAGES C CXX VERSION 1.3.0 )
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
# # # algorithm # #

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(chase_algorithm INTERFACE)

include(GNUInstallDirs)

target_include_directories( chase_algorithm INTERFACE
  "$<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}>"
  $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>  # <prefix>/include/mylib
)

target_compile_features(chase_algorithm INTERFACE cxx_auto_type)

option( CHASE_OUTPUT "ChASE will provide output at each iteration")
if( CHASE_OUTPUT )
    target_compile_definitions( chase_algorithm  INTERFACE "-DCHASE_OUTPUT" )
endif()

option(ENABLE_NSIGHT "Enable profiling with Nvidia Nsight Systems" OFF)

install( TARGETS chase_algorithm
  EXPORT chase-headers
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
  )


install(DIRECTORY algorithm DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
  FILES_MATCHING
    PATTERN "*.hpp"
    PATTERN "*.inc"
)

install(EXPORT chase-headers
  NAMESPACE ChASE::
  FILE chase-header.cmake
  EXPORT_LINK_INTERFACE_LIBRARIES
  DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_NAME}
  )

# # ChASE-MPI # #
add_subdirectory( "ChASE-MPI/")

# # Tests
add_executable( "chase_driver" tests/noinput.cpp )
if(TARGET chase_cuda )
    enable_language(CUDA)
    target_link_libraries(chase_driver chase_mpi chase_cuda)
    target_compile_definitions(chase_driver PRIVATE USE_GPU=1)
else()
    target_link_libraries(chase_driver chase_mpi)
endif()

add_subdirectory("interface")

# Examples
option(BUILD_WITH_EXAMPLES "Build the examples" OFF)
if(BUILD_WITH_EXAMPLES)
    message(STATUS "Building the examples of ChASE")
    add_subdirectory("./examples")
endif()


option(ENABLE_TESTS "Enable unit tests." OFF)
if(ENABLE_TESTS)
    MESSAGE("Test enabled. Finding GoogleTests.")
    include(${CMAKE_SOURCE_DIR}/cmake/external/Gtest/FetchGtest.cmake)
    include(CTest)

    set(MPI_RUN mpirun CACHE STRING "MPI runner (mpirun/srun)")
    set(MPI_RUN_ARGS  CACHE STRING "")
    set(MPI_TEST ON CACHE BOOL "Run test with mpi")
    add_subdirectory(tests)
endif()

# Documentation
option(BUILD_WITH_DOCS "Build the examples" OFF)
if(BUILD_WITH_DOCS)
    message(STATUS "Building Documentation of ChASE")
    add_subdirectory("./docs")
endif()

# Install ChASE as a CMake package
include(CMakePackageConfigHelpers)

if(TARGET chase_cuda )
    configure_package_config_file(
    "cmake/Config_CUDA.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/chase-config.cmake"
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_NAME}
)

else()
    configure_package_config_file(
    "cmake/Config.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/chase-config.cmake"
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_NAME}
)

endif()

install( FILES
  "${CMAKE_CURRENT_BINARY_DIR}/chase-config.cmake"
  DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_NAME}
  )

# If variable not defined, set it to default value in order to comply with CMake Policy CMP0104
if (NOT DEFINED CMAKE_CUDA_ARCHITECTURES)
    set (CMAKE_CUDA_ARCHITECTURES "80")
endif()

# If CUDA compute compatibility of your GPU is 8.6 (e.g. RTX 3090) you should build with
# -DCMAKE_CUDA_ARCHITECTURES=86. In the case you want to build code for more than one
# CUDA compute capability (e.g. 70, 75, 80 and 86) then build with
# -DCMAKE_CUDA_ARCHITECTURES="70;75;80;86"

# If CMAKE_VERSION < 3.18 then CMake is not compliant with CMAKE policy CMP0104 (introduced
# in CMake 3.18) which defines that the variable CMAKE_CUDA_ARCHITECTURES has to be initialized.
# In that case, the code generation flag has to be set manually.
# For simplicity and compatibility with newer (3.18+) CMake version, the CMAKE_CUDA_ARCHITECTURES
# variable has to be always set, no matter the cmake version.
if (${CMAKE_VERSION} VERSION_LESS "3.18.0")
    message("CMAKE version less than 3.18. Using old CMAKE CUDA policy for defining CUDA code generation flags")
    foreach(CODE ${CMAKE_CUDA_ARCHITECTURES})
        set (CMAKE_CUDA_FLAGS "${CMAKE_CUDA_FLAGS} -gencode arch=compute_${CODE},code=sm_${CODE}")
    endforeach()
endif()

message("-- Configuring the build of CUDA with the compute capability of ${CMAKE_CUDA_ARCHITECTURES}")

if(NOT DEFINED CUDA_nvToolsExt_LIBRARY)
    set (CUDA_nvToolsExt_LIBRARY "")
    if(ENABLE_NSIGHT)
        message(STATUS "No Nsight Profiling Tools detected")
        set(ENABLE_NSIGHT OFF CACHE BOOL "" FORCE)
    endif()
endif()

add_library( chase_cuda "kernels/shift.cu" )

set_target_properties( chase_cuda
      PROPERTIES
      CUDA_RESOLVE_DEVICE_SYMBOLS ON
    CUDA_SEPARABLE_COMPILATION ON)

target_include_directories(
      chase_cuda
      PUBLIC #$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include/>
             $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/ChASE-MPI>)
# CUDA_INCLUDE_DIRS is thought to contain also the include files for cublas, cusolver and curand, but it is not the case
target_include_directories( chase_cuda INTERFACE
    ${CUDA_INCLUDE_DIRS}
    ${MATH_LIBS_INCLUDE_DIR}
    )
target_link_libraries( chase_cuda INTERFACE
    ${CUDA_CUBLAS_LIBRARIES}
    ${CUDA_cusolver_LIBRARY}
    ${CUDA_curand_LIBRARY}
    ${CUDA_nvToolsExt_LIBRARY}
    )
target_link_libraries( chase_mpi INTERFACE
    ${CUDA_nvToolsExt_LIBRARY}
    )
target_link_libraries( chase_seq INTERFACE
    ${CUDA_nvToolsExt_LIBRARY}
    )

target_compile_definitions( chase_cuda INTERFACE
   "-DHAS_CUDA"
        )

# Enforce that ENABLE_TUNING requires ENABLE_UM
if(ENABLE_TUNING AND NOT ENABLE_UM)
    message(FATAL_ERROR "ENABLE_TUNING requires ENABLE_UM to be ON. Please set -DENABLE_UM=ON.")
endif()

if(ENABLE_UM)
    message(INFO "Unified Memory (UM) enabled.")
    target_compile_definitions( chase_cuda INTERFACE
              "-DHAS_UM"
              )
    if(ENABLE_TUNING)
        message(INFO "Unified Memory Tuning (UMT) enabled.")
        target_compile_definitions( chase_cuda INTERFACE
                  "-DHAS_TUNING"
                  )
    endif()
endif()

if(ENABLE_CUDA_AWARE_MPI)
    target_compile_definitions( chase_cuda INTERFACE
            "-DCUDA_AWARE"
          )

    if(ENABLE_NCCL)
        find_package( NCCL REQUIRED )
    endif()

    if(NCCL_FOUND)
        target_link_libraries(chase_cuda INTERFACE
              ${NCCL_LIBRARIES}
              )
        target_include_directories( chase_cuda INTERFACE
              ${NCCL_INCLUDE_DIRS}
              )
        target_compile_definitions( chase_cuda INTERFACE
              "-DHAS_NCCL"
          )
    endif()
endif()

if(ENABLE_NSIGHT)
    target_compile_definitions(chase_cuda INTERFACE USE_NSIGHT)
    target_compile_definitions(chase_seq  INTERFACE USE_NSIGHT)
    target_compile_definitions(chase_mpi INTERFACE USE_NSIGHT)
    target_compile_definitions(chase_algorithm INTERFACE USE_NSIGHT)
endif()

install( TARGETS chase_cuda
    EXPORT chase-cuda
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    )

install(EXPORT chase-cuda
    NAMESPACE ChASE::
    FILE chase-cuda.cmake
    EXPORT_LINK_INTERFACE_LIBRARIES
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_NAME}
    )

else()
message(STATUS "No CUDA support")
if(ENABLE_NSIGHT)
    message(STATUS "No CUDA support for Nsight Profiling Tools")
endif()
endif()

install( TARGETS chase_seq
  EXPORT chase-seq
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
  )

install(EXPORT chase-seq
  NAMESPACE ChASE::
  FILE chase-seq.cmake
  EXPORT_LINK_INTERFACE_LIBRARIES
  DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_NAME}
  )

install( TARGETS chase_mpi
  EXPORT chase-mpi
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
  )

install(DIRECTORY . DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/ChASE-MPI/
  FILES_MATCHING
    PATTERN "*.hpp"
    PATTERN "*.inc"
)

install( FILES  ${CMAKE_BINARY_DIR}/fortran_mangle.h
  DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
  )

install(EXPORT chase-mpi
  NAMESPACE ChASE::
  FILE chase-mpi.cmake
  EXPORT_LINK_INTERFACE_LIBRARIES
  DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_NAME}
  )
