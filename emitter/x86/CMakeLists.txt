# This file if part of the genm-opt project.
# Licensing information can be found in the LICENSE file.
# (C) 2018 Nandor Licker. All rights reserved.


add_library(x86_emitter
  x86emitter.cpp
)
target_link_libraries(x86_emitter emitter)
