// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#pragma once

#include <set>

#include "core/inst_visitor.h"

class SymbolicContext;
class SymbolicHeap;
class SymbolicValue;
class SymbolicPointer;
class ReferenceGraph;
class SCCNode;
class SymbolicFrame;



/**
 * Symbolically evaluate an instruction.
 */
class SymbolicLoop final {
public:
  SymbolicLoop(ReferenceGraph &refs, SymbolicContext &ctx)
    : refs_(refs)
    , ctx_(ctx)
  {
  }

  /// Over-approximate the effects of a loop.
  void Evaluate(
      SymbolicFrame &frame,
      const std::set<Block *> &active,
      SCCNode *node
  );

private:
  /// Accurate evaluation.
  void Evaluate(
      SymbolicFrame &frame,
      Block *from,
      SCCNode *node,
      Block *block
  );
  /// Over-approximation.
  void Approximate(
      SymbolicFrame &frame,
      const std::set<Block *> &active,
      SCCNode *node
  );
  /// Approximate a call.
  bool Approximate(CallSite &call);

private:
  /// Reference to the cached information.
  ReferenceGraph &refs_;
  /// Context the instruction is being evaluated in.
  SymbolicContext &ctx_;
};
