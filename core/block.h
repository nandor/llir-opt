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
  using reverse_iterator = Chain<Inst>::reverse_iterator;
  using const_iterator = Chain<Inst>::const_iterator;
  using const_reverse_iterator = Chain<Inst>::const_reverse_iterator;

public:
  Block(const std::string_view name);

  void AddInst(Inst *inst);

  std::string_view GetName() const { return name_; }

  // Checks if the block is empty.
  bool IsEmpty() const { return insts_.empty(); }

  // Iterator over the instructions.
  iterator begin() { return insts_.begin(); }
  iterator end() { return insts_.end(); }
  const_iterator begin() const { return insts_.begin(); }
  const_iterator end() const { return insts_.end(); }
  reverse_iterator rbegin() { return insts_.rbegin(); }
  reverse_iterator rend() { return insts_.rend(); }
  const_reverse_iterator rbegin() const { return insts_.rbegin(); }
  const_reverse_iterator rend() const { return insts_.rend(); }

private:
  /// Name of the block.
  std::string name_;
  /// Parent function.
  Func *func_;
  /// Chain of instructions.
  Chain<Inst> insts_;
};
