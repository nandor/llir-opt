// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#pragma once

#include "llvm/TableGen/Record.h"



/**
 * Writes a list of generic per-instruction attributes.
 */
class GetCompareWriter {
public:
  GetCompareWriter(llvm::RecordKeeper &records) : records_(records) {}

  void run(llvm::raw_ostream &OS);

private:
  llvm::RecordKeeper &records_;
};
