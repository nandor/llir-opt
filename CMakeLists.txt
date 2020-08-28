# This file if part of the llir-opt project.
# Licensing information can be found in the LICENSE file.
# (C) 2018 Nandor Licker. All rights reserved.

################################################################################
# llir-opt
################################################################################
cmake_minimum_required(VERSION 3.10.0)
project(llir-opt)

find_package(LLVM CONFIG)
find_package(GTest)
find_package(Doxygen)

enable_testing()
enable_language(C CXX ASM)

# Set up project version.
set(VERSION_MAJOR   0   CACHE STRING "Project major version number.")
set(VERSION_MINOR   0   CACHE STRING "Project minor version number.")
set(VERSION_PATCH   1   CACHE STRING "Project patch version number.")
mark_as_advanced(VERSION_MAJOR VERSION_MINOR VERSION_PATCH)

if (DOXYGEN_FOUND)
  set(DOXYFILE_IN ${CMAKE_CURRENT_SOURCE_DIR}/docs/Doxyfile.in)
  set(DOXYFILE ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile)

  configure_file(${DOXYFILE_IN} ${DOXYFILE} @ONLY)

  add_custom_command(
      OUTPUT  ${CMAKE_CURRENT_BINARY_DIR}/html/index.html
      COMMAND ${DOXYGEN_EXECUTABLE} ${DOXYFILE}
      WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
      VERBATIM
  )

  add_custom_target(
      docs
      DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/html/index.html
  )
endif (DOXYGEN_FOUND)

if (LLVM_FOUND)
  message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
  message(STATUS "Using LLVMConfig.cmake in: ${LLVM_DIR}")

  include_directories(SYSTEM ${LLVM_INCLUDE_DIRS})
  link_directories(${LLVM_LIBRARY_DIRS})
  add_definitions(${LLVM_DEFINITIONS})
  llvm_map_components_to_libnames(LLVM_LIBS
      aggressiveinstcombine
      asmprinter
      codegen
      mc
      selectiondag
      support
      x86codegen
      x86desc
      x86info
      llircodegen
      llirdesc
      llirinfo
  )

  # Set up C++ options.
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

  # DYLD_LIBRARY_PATH for Darwin.
  set(CMAKE_INSTALL_RPATH "${CMAKE_INSTALL_PREFIX}/lib")
  set(CMAKE_INSTALL_NAME_DIR "${CMAKE_INSTALL_PREFIX}/lib")
  set(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)

  # Auxiliary utilities.
  add_subdirectory(util)
  add_subdirectory(core)
  add_subdirectory(passes)
  add_subdirectory(emitter)
  add_subdirectory(stats)

  # Tools and executables.
  add_subdirectory(tools)
endif (LLVM_FOUND)
