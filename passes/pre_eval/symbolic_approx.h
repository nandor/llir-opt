// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#pragma once

#include <set>

#include "core/inst_visitor.h"

class SymbolicContext;
class SymbolicHeap;
class ReferenceGraph;
class BlockEvalNode;



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

  /// Over-approximate the effects of a bypassed branch.
  void Approximate(
      std::set<BlockEvalNode *> bypassed,
      std::set<SymbolicContext *> contexts
  );

private:
  /// Over-approximate the effects of a particular function.
  bool Approximate(CallSite &call, Func &func);

  /// Try to resolve a mov to a constant.
  void Resolve(MovInst &mov);

private:
  /// Reference to the cached information.
  ReferenceGraph &refs_;
  /// Context the instruction is being evaluated in.
  SymbolicContext &ctx_;
};
