# This file if part of the llir-opt project.
# Licensing information can be found in the LICENSE file.
# (C) 2018 Nandor Licker. All rights reserved.

add_executable(llir-tblgen
  get_bitcode.cpp
  get_cast.cpp
  get_clone.cpp
  get_class.cpp
  get_compare.cpp
  get_instruction.cpp
  get_parser.cpp
  get_printer.cpp
  inst_gen.cpp
  util.cpp
)
llvm_map_components_to_libnames(GENM_TBLGEN_LIBS tablegen)
target_link_libraries(llir-tblgen ${GENM_TBLGEN_LIBS})
