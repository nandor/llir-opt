// This file if part of the genm-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#pragma once

#include <string>
#include <string_view>
#include <vector>
#include "adt/chain.h"
#include "core/block.h"
#include "core/inst.h"

class Func;



/**
 * Basic block.
 */
class Block : public ChainNode<Block> {
public:
  Block(const std::string_view name);

  void AddInst(Inst *inst);

  std::string_view GetName() const { return name_; }

private:
  /// Name of the block.
  std::string name_;
  /// Parent function.
  Func *func_;
  /// First instruction.
  Inst *fst_;
  /// Last instruction.
  Inst *lst_;
};
