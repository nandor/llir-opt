# This file if part of the llir-opt project.
# Licensing information can be found in the LICENSE file.
# (C) 2018 Nandor Licker. All rights reserved.

add_library(emitter
  annot_printer.cpp
  call_lowering.cpp
  data_printer.cpp
  emitter.cpp
  isel.cpp
  isel_mapping.cpp
  runtime_printer.cpp
)
add_dependencies(emitter core)

add_subdirectory(aarch64)
add_subdirectory(coq)
add_subdirectory(ppc)
add_subdirectory(riscv)
add_subdirectory(x86)
