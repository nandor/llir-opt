// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#pragma once

#include <unordered_map>

#include "core/cast.h"
#include "core/func.h"
#include "core/inst.h"
#include "core/insts.h"
#include "core/analysis/kildall.h"
#include "passes/local_const/graph.h"


/// Forward declarations.
class LCContext;

/**
 * Store to load propagation.
 */
class StoreElimination {
private:
  /// Type to identify an element of an allocation.
  using Element = std::pair<ID<LCAlloc>, uint64_t>;

  /// Kill/Gen forward decl.
  class KillGen;

  /// Set describing reachable writes.
  class Set {
  public:
    Set() {}

    bool Contains(ID<LCAlloc> alloc, uint64_t index) const
    {
      return elems_.find({ alloc, index }) != elems_.end();
    }

    bool Contains(ID<LCAlloc> alloc) const
    {
      return allocs_.Contains(alloc);
    }

    void Minus(const KillGen &that);

    void Union(const KillGen &that);

    void Union(const Set &that);

    bool operator==(const Set &that) const
    {
      return allocs_ == that.allocs_ && elems_ == that.elems_;
    }

    void dump(llvm::raw_ostream &os);

  private:
    friend class Analysis;
    BitSet<LCAlloc> allocs_;
    std::set<Element> elems_;
  };

  /// Reachable defs at each node.
  struct KillGen {
    BitSet<LCAlloc> Allocs;
    std::set<Element> Elems;

    void dump(llvm::raw_ostream &os);

    // Set difference: this - that.
    void Minus(const KillGen &that);
    // Set union: this U that.
    void Union(const KillGen &that);
  };

public:
  /// Initialises the transformation.
  StoreElimination(Func &func, LCContext &context) : solver_(func, context) {}

  /// Propagates.
  void Eliminate() { return solver_.Solve(); }

private:
  class Solver : public KillGenSolver<Set, KillGen, KillGen, Direction::BACKWARD> {
  public:
    Solver(Func &func, LCContext &context)
      : KillGenSolver(func), context_(context)
    {
    }

    void Build(Inst &inst);

    void Traverse(Inst *inst, const Set &set);

  private:
    void BuildCall(Inst *I);
    void BuildLongJmp(Inst *I);
    void BuildAlloc(Inst *I);
    void BuildStore(StoreInst *st, LCSet *addr);
    void BuildClobber(Inst *I, LCSet *addr);
    void BuildGen(Inst *I, LCSet *addr);
    void BuildExtern(Inst *I, InstInfo &kg);
    void BuildRoots(Inst *I, InstInfo &kg);
    void BuildReturn(Inst *I, InstInfo &kg);

  private:
    LCContext &context_;
  };

  Solver solver_;
};
