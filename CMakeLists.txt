# This file if part of the genm-opt project.
# Licensing information can be found in the LICENSE file.
# (C) 2018 Nandor Licker. All rights reserved.

################################################################################
# genm-opt
################################################################################
cmake_minimum_required(VERSION 3.10.0)
project(genm-opt)



################################################################################
# Set up LLVM.
################################################################################
find_package(LLVM REQUIRED CONFIG)
message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
message(STATUS "Using LLVMConfig.cmake in: ${LLVM_DIR}")

include_directories(${LLVM_INCLUDE_DIRS})
add_definitions(${LLVM_DEFINITIONS})

llvm_map_components_to_libnames(LLVM_LIBS all)

################################################################################
# Set up C++ options.
################################################################################
add_compile_options(${LLVM_CXXFLAGS} -std=c++17 -fno-rtti)
include_directories(${LLVM_INCLUDE_DIRS})
include_directories(${CMAKE_CURRENT_SOURCE_DIR})
link_directories(${LLVM_LIBRARY_DIRS})

################################################################################
# genm executable.
################################################################################
add_subdirectory(core)
add_subdirectory(emitter)
add_executable(genm main.cpp)
target_link_libraries(genm core x86_emitter ${LLVM_LIBS})
