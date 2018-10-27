// This file if part of the genm-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include "core/block.h"



// -----------------------------------------------------------------------------
Block::Block(const std::string_view name)
  : name_(name)
{
}

// -----------------------------------------------------------------------------
void Block::AddInst(Inst *i)
{
  insts_.push_back(*i);
}
