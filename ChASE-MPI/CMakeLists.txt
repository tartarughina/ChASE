# -*- Mode: cmake -*-

# Specify the minimum version for CMake
cmake_minimum_required( VERSION 3.8 )

option(ENABLE_CUDA_AWARE_MPI "Enable CUDA aware MPI for collective communications" ON)
option(ENABLE_NCCL "Enable Nvidia NCCL for collective communications" ON)
option(ENABLE_MPI_IO "Enable MPI IO to read Hamiltonian matrix from local" ON)
option(ENABLE_UM "Enable Unified Memory for CUDA" OFF)
option(ENABLE_TUNING "Enable UM Tuning" OFF)

add_library( chase_seq INTERFACE )
target_link_libraries(chase_seq INTERFACE chase_algorithm)
target_compile_definitions(chase_seq INTERFACE NO_MPI=1 )


add_library( chase_mpi INTERFACE )
target_link_libraries(chase_mpi INTERFACE chase_algorithm)

#underscores for FORTRAN
enable_language( Fortran )
include( FortranCInterface )
FortranCInterface_HEADER( ${CMAKE_BINARY_DIR}/fortran_mangle.h
  MACRO_NAMESPACE "FC_"
  SYMBOL_NAMESPACE "FC_"
  )

list(APPEND includePath ${CMAKE_BINARY_DIR} ${CMAKE_CURRENT_SOURCE_DIR})

target_include_directories( chase_seq INTERFACE
  "$<BUILD_INTERFACE:${includePath}>"
  $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>  # <prefix>/include/ChASE-MPI
)

target_include_directories( chase_mpi INTERFACE
  "$<BUILD_INTERFACE:${includePath}>"
  $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>  # <prefix>/include/ChASE-MPI
)

# # BLAS / LAPACK # #
find_package( BLAS   REQUIRED )
find_package( LAPACK REQUIRED )
find_package( MPI    REQUIRED )

find_package( SCALAPACK )

target_include_directories( chase_seq INTERFACE
  ${MPI_CXX_INCLUDE_PATH}
  )

target_link_libraries( chase_seq INTERFACE
  ${BLAS_LIBRARIES}
  ${LAPACK_LIBRARIES}
  ${MPI_CXX_LIBRARIES}
  )

if(SCALAPACK_FOUND)
    target_link_libraries( chase_seq INTERFACE
   ${SCALAPACK_LIBRARIES}
   )
endif()

target_compile_options( chase_seq INTERFACE
  ${MPI_CXX_COMPILE_FLAGS}
  )

target_include_directories( chase_mpi INTERFACE
  ${MPI_CXX_INCLUDE_PATH}
  )

target_link_libraries( chase_mpi INTERFACE
  ${BLAS_LIBRARIES}
  ${LAPACK_LIBRARIES}
  ${MPI_CXX_LIBRARIES}
  )

target_compile_options( chase_mpi INTERFACE
  ${MPI_CXX_COMPILE_FLAGS}
  )
if(SCALAPACK_FOUND)
    target_link_libraries( chase_mpi INTERFACE
        ${SCALAPACK_LIBRARIES}
        )
    target_compile_definitions( chase_mpi INTERFACE
   "-DHAS_SCALAPACK"
 )
endif()

target_compile_definitions( chase_mpi INTERFACE
   "-DUSE_MPI"
        )

if(ENABLE_MPI_IO)
    target_compile_definitions( chase_mpi INTERFACE
     "-DUSE_MPI_IO"
 )
endif()

include(CheckLanguage)
check_language(CUDA)
if(CMAKE_CUDA_COMPILER)

    if (${CMAKE_VERSION} VERSION_LESS_EQUAL "3.18.0")
        find_package(CUDA REQUIRED)
    elseif(${CMAKE_VERSION} VERSION_GREATER "3.18.0")
        string(REPLACE "compilers/bin/nvcc" "cmake" NVHPC_CMAKE_DIR ${CMAKE_CUDA_COMPILER})
        set(CMAKE_PREFIX_PATH ${NVHPC_CMAKE_DIR})
        message(STATUS "NVHPC CMake Directory: ${NVHPC_CMAKE_DIR}")
        message(STATUS "CMake Prefix Path set to: ${CMAKE_PREFIX_PATH}")
        find_package(NVHPC REQUIRED COMPONENTS CUDA MATH)
    endif()

    enable_language(CUDA)

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
    string(REGEX REPLACE "(.+)(\/lib64)$" "\\1/include" NVHPC_CUDA_INCLUDE_DIR ${NVHPC_CUDA_LIBRARY_DIR})
    string(REGEX REPLACE "(.+)(\/lib64)$" "\\1/include" NVHPC_MATH_INCLUDE_DIR ${NVHPC_MATH_LIBRARY_DIR})
    target_include_directories( chase_cuda INTERFACE
    ${NVHPC_CUDA_INCLUDE_DIR} ${NVHPC_MATH_INCLUDE_DIR})

    target_link_libraries( chase_cuda INTERFACE
    NVHPC::CUBLAS
    NVHPC::CUSOLVER
    NVHPC::CURAND
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

    if(ENABLE_CUDA_AWARE_MPI)
        target_compile_definitions( chase_cuda INTERFACE
            "-DCUDA_AWARE"
          )

        if(ENABLE_UM)
            target_compile_definitions( chase_cuda INTERFACE
                  "-DHAS_UM"
                )
        endif()

        if(ENABLE_TUNING)
            target_compile_definitions( chase_cuda INTERFACE
                  "-DHAS_TUNING"
                )
        endif()

        if(ENABLE_NCCL)
            find_package( NVHPC REQUIRED COMPONENTS NCCL )
            find_library(NCCL_LIBRARY
                NAMES nccl
                HINTS ${NVHPC_NCCL_LIBRARY_DIR}
            )
            string(REGEX REPLACE "(.+)(\/lib)$" "\\1/include" NCCL_INCLUDE_DIR ${NVHPC_NCCL_LIBRARY_DIR})
            target_link_libraries(chase_cuda INTERFACE
          ${NCCL_LIBRARY}
          )
            target_include_directories( chase_cuda INTERFACE
          ${NCCL_INCLUDE_DIR}
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

