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
class SymbolicApprox final {
public:
  SymbolicApprox(ReferenceGraph &refs, SymbolicContext &ctx)
    : refs_(refs)
    , ctx_(ctx)
  {
  }

  /// Over-approximate the effects of a call.
  bool Approximate(CallSite &call);

  /// Over-approximate the effects of a bypassed branch.
  void Approximate(
      SymbolicFrame &frame,
      std::set<SCCNode *> bypassed,
      std::set<SymbolicContext *> contexts
  );

private:
  /// Over-approximate the effects of a particular function.
  bool Approximate(CallSite &call, Func &func);

  /// Extract references from a value.
  void Extract(
      const SymbolicValue &value,
      std::set<Global *> &pointers,
      std::set<std::pair<unsigned, unsigned>> &frames,
      std::set<std::pair<unsigned, CallSite *>> &sites
  );
  /// Extract references from a call.
  void Extract(Func &func, const SymbolicValue &value, SymbolicPointer &ptr);

  /// Try to resolve a mov to a constant.
  void Resolve(MovInst &mov);

private:
  /// Reference to the cached information.
  ReferenceGraph &refs_;
  /// Context the instruction is being evaluated in.
  SymbolicContext &ctx_;
};
