# This file if part of the llir-opt project.
# Licensing information can be found in the LICENSE file.
# (C) 2018 Nandor Licker. All rights reserved.

add_executable(llir-tblgen
  get_instruction.cpp
  inst_gen.cpp
)
llvm_map_components_to_libnames(GENM_TBLGEN_LIBS tablegen)
target_link_libraries(llir-tblgen ${GENM_TBLGEN_LIBS})
