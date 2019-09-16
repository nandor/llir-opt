# This file if part of the genm-opt project.
# Licensing information can be found in the LICENSE file.
# (C) 2018 Nandor Licker. All rights reserved.

add_executable(genm-tblgen
  TableGen.cpp
)
llvm_map_components_to_libnames(GENM_TBLGEN_LIBS tablegen)
target_link_libraries(genm-tblgen ${GENM_TBLGEN_LIBS})
