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
private:
  using iterator = Chain<Inst>::iterator;
  using const_iterator = Chain<Inst>::const_iterator;

public:
  Block(const std::string_view name);

  void AddInst(Inst *inst);

  std::string_view GetName() const { return name_; }

  // Iterator over the instructions.
  iterator begin() { return insts_.begin(); }
  iterator end() { return insts_.end(); }
  const_iterator begin() const { return insts_.begin(); }
  const_iterator end() const { return insts_.end(); }

private:
  /// Name of the block.
  std::string name_;
  /// Parent function.
  Func *func_;
  /// Chain of instructions.
  Chain<Inst> insts_;
};
