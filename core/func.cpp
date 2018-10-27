// This file if part of the genm-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include "core/func.h"
#include "core/block.h"



// -----------------------------------------------------------------------------
Func::Func(Prog *prog, const std::string &name)
  : prog_(prog)
  , name_(name)
{
}

// -----------------------------------------------------------------------------
Block *Func::AddBlock(const std::string &name)
{
  Block *block = new Block(name);
  blocks_.push_back(*block);
  return block;
}

// -----------------------------------------------------------------------------
Block *Func::AddBlock()
{
  Block *block = new Block(".LBB" + std::to_string(blocks_.size()));
  blocks_.push_back(*block);
  return block;
}
