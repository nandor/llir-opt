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
class StorePropagation {
private:
  /// Type to identify an element of an allocation.
  using Element = std::pair<ID<LCAlloc>, LCIndex>;

  /// Kill/Gen forward decl.
  class Gen;
  class Kill;

  /// Set describing reachable writes.
  class Set {
  public:
    Set() {}

    StoreInst *Find(const Element &elem) const
    {
      if (auto it = defs_.find(elem); it != defs_.end()) {
        return ::dyn_cast_or_null<StoreInst>(it->second);
      }
      return nullptr;
    }

    void Minus(const Kill &that);

    void Union(const Gen &that);

    void Union(const Set &that);

    bool operator==(const Set &that) const { return defs_ == that.defs_; }

    void dump(llvm::raw_ostream &os);

  private:
    friend class Analysis;
    std::map<Element, Inst *> defs_;
  };

  /// Reachable defs at each node.
  struct Gen {
    void Minus(const Kill &that);
    void Union(const Gen &that);

    void dump(llvm::raw_ostream &os);

    /// Gens of the block.
    std::map<Element, Inst *> Elems;
  };

  /// Reachable kills at each node.
  struct Kill {
    /// Kill of all elements of this allocation.
    BitSet<LCAlloc> Allocs;
    /// Kill individual elements in the allocation.
    std::set<Element> Elems;

    void dump(llvm::raw_ostream &os);

    void Union(const Kill &that);
  };

public:
  /// Initialises the transformation.
  StorePropagation(Func &func, LCContext &context) : solver_(func, context) {}

  /// Propagates.
  void Propagate() { return solver_.Solve(); }

private:
  class Solver : public KillGenSolver<Set, Gen, Kill, Direction::FORWARD> {
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
    void BuildExtern(Inst *I, InstInfo &kg);
    void BuildRoots(Inst *I, InstInfo &kg);

  private:
    LCContext &context_;
  };

  Solver solver_;
};
