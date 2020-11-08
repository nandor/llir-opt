# This file if part of the llir-opt project.
# Licensing information can be found in the LICENSE file.
# (C) 2018 Nandor Licker. All rights reserved.


add_library(ppc_emitter
  ppcannot_printer.cpp
  ppccall.cpp
  ppcemitter.cpp
  ppcisel.cpp
  ppcruntime_printer.cpp
)
target_link_libraries(ppc_emitter emitter)
