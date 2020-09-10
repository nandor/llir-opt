// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#pragma once

#include "passes/pre_eval/symbolic_value.h"

class Inst;
class StoreInst;



/**
 * Store to keep track of constants and writes to the heap.
 */
class SymbolicContext final {
public:

  SymbolicValue operator[](Inst *inst) const;

  bool IsStoreFolded(StoreInst *st) const;
private:

};
