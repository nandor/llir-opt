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
 * Returns true if the function is an allocation site.
 */
bool IsAllocation(Func &func);

/**
 * Symbolically evaluate an instruction.
 */
class SymbolicApprox final {
public:
  SymbolicApprox(ReferenceGraph &refs, SymbolicHeap &heap, SymbolicContext &ctx)
    : refs_(refs)
    , heap_(heap)
    , ctx_(ctx)
  {
  }

  /// Over-approximate the effects of a call.
  bool Approximate(CallSite &call);

  /// Over-approximate the effects of a bypassed branch.
  void Approximate(
      SymbolicFrame &frame,
      const std::set<SCCNode *> &bypassed,
      const std::set<SymbolicContext *> &contexts
  );

private:
  /// Over-approximate the effects of a call.
  bool ApproximateCall(CallSite &call);

  /// Approximate the effects of a group of instructions.
  std::pair<bool, bool> ApproximateNodes(
      const std::set<CallSite *> &calls,
      const std::set<CallSite *> &allocs,
      SymbolicValue &refs,
      SymbolicContext &ctx
  );

  /// Propagate information to landing pad.
  bool Raise(const SymbolicValue &taint);

  /// Try to resolve a mov to a constant.
  void Resolve(MovInst &mov, const SymbolicValue &taint);

  /// Malloc model.
  bool Malloc(CallSite &call, const std::optional<APInt> &size);

  /// Realloc model.
  bool Realloc(
      CallSite &call,
      const SymbolicValue &ptr,
      const std::optional<APInt> &size
  );

private:
  /// Reference to the cached information.
  ReferenceGraph &refs_;
  /// Reference to the heap.
  SymbolicHeap &heap_;
  /// Context the instruction is being evaluated in.
  SymbolicContext &ctx_;
};
