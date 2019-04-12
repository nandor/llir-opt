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

llvm_map_components_to_libnames(LLVM_LIBS all)


################################################################################
# Set up C++ options.
################################################################################
add_compile_options(${LLVM_CXXFLAGS} -std=c++17 -fno-rtti -Werror)
add_compile_options(
  -Wextra
  -Wno-unused-parameter
  -Wno-sign-compare
)
include_directories(${CMAKE_CURRENT_SOURCE_DIR})


################################################################################
# DYLD_LIBRARY_PATH for Darwin.
################################################################################
set(CMAKE_INSTALL_RPATH "${CMAKE_INSTALL_PREFIX}/lib")
set(CMAKE_INSTALL_NAME_DIR "${CMAKE_INSTALL_PREFIX}/lib")
set(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)


################################################################################
# genm executable.
################################################################################
add_subdirectory(core)
add_subdirectory(passes)
add_subdirectory(emitter)
add_executable(genm main.cpp)
target_link_libraries(genm core analysis passes x86_emitter ${LLVM_LIBS})
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
