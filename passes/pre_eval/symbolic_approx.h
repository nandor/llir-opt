// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#pragma once

#include <set>

#include "core/inst_visitor.h"
#include "passes/pre_eval/symbolic_value.h"

class SymbolicContext;
class SymbolicHeap;
class ReferenceGraph;
class DAGBlock;
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
      const std::set<DAGBlock *> &bypassed,
      const std::set<SymbolicContext *> &contexts
  );

private:
  /// Over-approximate the effects of a call.
  bool ApproximateCall(CallSite &call);

  /// Result of approximation.
  struct Approximation {
    bool Changed;
    bool Raises;
    SymbolicValue Taint;
    SymbolicValue Tainted;
  };

  /// Approximate the effects of a group of instructions.
  Approximation ApproximateNodes(
      const std::set<CallSite *> &calls,
      const std::set<CallSite *> &allocs,
      SymbolicValue &refs,
      SymbolicContext &ctx
  );

  /// Propagate information to landing pad.
  bool Raise(SymbolicContext &ctx, const SymbolicValue &taint);

  /// Try to resolve a mov to a constant.
  void Resolve(SymbolicFrame &frame, MovInst &mov, const SymbolicValue &taint);

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
