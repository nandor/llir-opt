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

include_directories(SYSTEM ${LLVM_INCLUDE_DIRS})
link_directories(${LLVM_LIBRARY_DIRS})
add_definitions(${LLVM_DEFINITIONS})

################################################################################
# Set up C++ options.
################################################################################
add_compile_options(${LLVM_CXXFLAGS} -std=c++17 -fno-rtti -Werror -Wextra)
include_directories(${CMAKE_CURRENT_SOURCE_DIR})

if("${CMAKE_CXX_COMPILER_ID}" MATCHES "Clang")
  add_compile_options(
    -Wno-unused-parameter
    -Wno-sign-compare
  )
elseif("${CMAKE_CXX_COMPILER_ID}" MATCHES "GNU")
  add_compile_options(
    -Wno-unused-parameter
    -Wno-sign-compare
    -Wno-implicit-fallthrough
    -Wno-maybe-uninitialized
  )
else()
  message(FATAL_ERROR "Unknown compiler: ${CMAKE_CXX_COMPILER_ID}")
endif()

################################################################################
# DYLD_LIBRARY_PATH for Darwin.
################################################################################
set(CMAKE_INSTALL_RPATH "${CMAKE_INSTALL_PREFIX}/lib")
set(CMAKE_INSTALL_NAME_DIR "${CMAKE_INSTALL_PREFIX}/lib")
set(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)

################################################################################
# Auxiliary utilities.
################################################################################
add_subdirectory(util)

################################################################################
# genm executable.
################################################################################
add_subdirectory(core)
add_subdirectory(passes)
add_subdirectory(emitter)
add_executable(genm main.cpp)
llvm_map_components_to_libnames(GENM_LIBS all)
target_link_libraries(genm passes x86_emitter core analysis ${GENM_LIBS})
install(
    TARGETS genm
    DESTINATION bin
)


################################################################################
# genm toolchain.
################################################################################
install(
    PROGRAMS
      ${CMAKE_SOURCE_DIR}/tools/genm-ar
      ${CMAKE_SOURCE_DIR}/tools/genm-ld
      ${CMAKE_SOURCE_DIR}/tools/genm-ranlib
    DESTINATION bin
)
