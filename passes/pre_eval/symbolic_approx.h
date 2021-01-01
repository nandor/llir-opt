// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#pragma once

#include "core/inst_visitor.h"

class SymbolicContext;
class SymbolicHeap;
class ReferenceGraph;



/**
 * Symbolically evaluate an instruction.
 */
class SymbolicApprox final {
public:
  SymbolicApprox(ReferenceGraph &refs, SymbolicContext &ctx)
    : refs_(refs)
    , ctx_(ctx)
  {
  }

  /// Over-approximate the effects of a loop.
  void Approximate(std::vector<Block *> blocks);

  /// Over-approximate the effects of a call.
  bool Approximate(CallSite &call);

private:
  /// Over-approximate the effects of a particular function.
  bool Approximate(CallSite &call, Func &func);

private:
  /// Reference to the cached information.
  ReferenceGraph &refs_;
  /// Context the instruction is being evaluated in.
  SymbolicContext &ctx_;
};
