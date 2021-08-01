# This file if part of the llir-opt project.
# Licensing information can be found in the LICENSE file.
# (C) 2018 Nandor Licker. All rights reserved.


add_library(aarch64_emitter
  aarch64annot_printer.cpp
  aarch64call.cpp
  aarch64emitter.cpp
  aarch64isel.cpp
  aarch64runtime_printer.cpp
)
target_link_libraries(aarch64_emitter emitter)
add_dependencies(aarch64_emitter llir-core)
