// This file if part of the genm-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#pragma once

#include "core/inst.h"

class Inst;
class Func;



/**
 * Basic block.
 */
class Block {
public:
  Block();

  Inst *AddInst(Inst::Type op);

private:
  /// Parent function.
  Func *func_;
  /// First instruction.
  Inst *fst_;
  /// Last instruction.
  Inst *lst_;
};
