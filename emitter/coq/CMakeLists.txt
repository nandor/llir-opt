# This file if part of the llir-opt project.
# Licensing information can be found in the LICENSE file.
# (C) 2018 Nandor Licker. All rights reserved.


add_library(coq_emitter
  coqemitter.cpp
)
target_link_libraries(coq_emitter emitter)
add_dependencies(coq_emitter llir-core)
