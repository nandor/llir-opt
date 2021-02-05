// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#pragma once

#include <llvm/Option/ArgList.h>





/**
 * OPT_#ID value for all options.
 */
enum {
  OPT_INVALID = 0,
  #define OPTION(_1, _2, ID, _4, _5, _6, _7, _8, _9, _10, _11, _12) OPT_##ID,
  #include "LdOptions.inc"
  #undef OPTION
};


/**
 * Option parser for the linker.
 */
class OptionTable : public llvm::opt::OptTable {
public:
  OptionTable();

  llvm::Expected<llvm::opt::InputArgList>
  Parse(llvm::ArrayRef<const char *> argv);
};
