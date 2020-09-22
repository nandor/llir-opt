# This file if part of the llir-opt project.
# Licensing information can be found in the LICENSE file.
# (C) 2018 Nandor Licker. All rights reserved.

add_library(emitter
  data_printer.cpp
  emitter.cpp
  isel.cpp
  isel_mapping.cpp
)

add_subdirectory(coq)
add_subdirectory(x86)
