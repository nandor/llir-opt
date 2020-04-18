# This file if part of the llir-opt project.
# Licensing information can be found in the LICENSE file.
# (C) 2018 Nandor Licker. All rights reserved.

################################################################################
# llir-opt
################################################################################
cmake_minimum_required(VERSION 3.10.0)
project(llir-opt)
enable_language(C CXX ASM)

################################################################################
# Set up GTest
################################################################################
find_package(GTest)
enable_testing()

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
add_compile_options(${LLVM_CXXFLAGS}
  -std=c++17
  -fno-rtti
  -fno-exceptions
  -Werror
  -Wextra
)
include_directories(${CMAKE_CURRENT_SOURCE_DIR})

if (LLVM_ENABLE_ASSERTIONS)
  add_compile_options(-D_DEBUG)
  if (NOT CMAKE_BUILD_TYPE STREQUAL "Debug")
    add_compile_options(-UNDEBUG)
  endif()
endif()

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(
    -Wno-unused-parameter
    -Wno-sign-compare
  )
elseif(CMAKE_CXX_COMPILER_ID MATCHES "GNU")
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
add_subdirectory(core)
add_subdirectory(passes)
add_subdirectory(emitter)
add_subdirectory(stats)

################################################################################
# llir-opt executable.
################################################################################
add_executable(llir-opt main.cpp)
target_link_libraries(llir-opt
    passes
    stats
    x86_emitter
    core
    analysis
    ${LLVM_LIBS}
)
install(
    TARGETS llir-opt
    DESTINATION bin
)

################################################################################
# llir-reduce executable.
################################################################################
add_executable(llir-reduce reduce.cpp)
target_link_libraries(llir-reduce
    passes
    core
    analysis
    ${LLVM_LIBS}
)
install(
    TARGETS llir-reduce
    DESTINATION bin
)

################################################################################
# llir-dump executable.
################################################################################
add_executable(llir-dump dump.cpp)
target_link_libraries(llir-dump
    core
    analysis
    ${LLVM_LIBS}
)
install(
    TARGETS llir-dump
    DESTINATION bin
)


################################################################################
# llir toolchain.
################################################################################
install(
    PROGRAMS
      ${CMAKE_SOURCE_DIR}/tools/llir-ar
      ${CMAKE_SOURCE_DIR}/tools/llir-ld
      ${CMAKE_SOURCE_DIR}/tools/llir-ranlib
      ${CMAKE_SOURCE_DIR}/tools/llir-reducer
    DESTINATION bin
)
