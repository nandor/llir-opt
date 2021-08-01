# This file if part of the llir-opt project.
# Licensing information can be found in the LICENSE file.
# (C) 2018 Nandor Licker. All rights reserved.


add_library(x86_emitter
  x86annot_printer.cpp
  x86call.cpp
  x86emitter.cpp
  x86isel.cpp
  x86runtime_printer.cpp
)
target_link_libraries(x86_emitter emitter)
add_dependencies(x86_emitter llir-core)
