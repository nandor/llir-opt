# This file if part of the llir-opt project.
# Licensing information can be found in the LICENSE file.
# (C) 2018 Nandor Licker. All rights reserved.


add_library(riscv_emitter
  riscvannot_printer.cpp
  riscvcall.cpp
  riscvemitter.cpp
  riscvisel.cpp
  riscvruntime_printer.cpp
)
target_link_libraries(riscv_emitter emitter)
add_dependencies(riscv_emitter llir-core)
