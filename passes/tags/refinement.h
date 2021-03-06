// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#pragma once

#include <queue>
#include <set>
#include <unordered_set>

#include "core/inst_visitor.h"
#include "core/target.h"
#include "passes/tags/tagged_type.h"



namespace tags {

class RegisterAnalysis;
struct DominatorCache;

/**
 * Helper to produce the initial types for known values.
 */
class Refinement : public InstVisitor<void> {
public:
  Refinement(
      RegisterAnalysis &analysis,
      const Target *target,
      bool banPolymorphism,
      Func &func
  );

  void Run();

private:
  /// Merge information at post-dom frontiers.
  void PullFrontier();

  /// Refine a type to a more precise one.
  void Refine(Block *parent, Ref<Inst> ref, const TaggedType &type);
  /// Refine a type to a more precise one, post-dominated by an edge.
  void Refine(Block *start, Block *end, Ref<Inst> ref, const TaggedType &type);
  /// Helper to refine a post-dominated definition.
  void Refine(Ref<Inst> ref, const TaggedType &type);
  /// Refine an argument to a join point.
  void RefineJoin(Ref<Inst> ref, const TaggedType &ty, Use &use, Type type);
  /// Create a mov for a cast.
  Ref<Inst> Cast(Ref<Inst> ref, const TaggedType &ty);
  /// Check if the refinement clarifies a monomorphic operator.
  bool IsNonPolymorphic(Ref<Inst> ref, const TaggedType &ty);
  /// Specialise a type downstream.
  void Specialise(
      Ref<Inst> ref,
      const Block *from,
      const std::vector<std::pair<TaggedType, Block *>> &branches
  );

private:
  /// Update the type and queue dependant.
  void RefineUpdate(Ref<Inst> inst, const TaggedType &type);
  /// Register a new split and queue dependants.
  void DefineUpdate(Ref<Inst> inst, const TaggedType &type);
  /// Queue the users of an instruction.
  void Queue(Ref<Inst> inst);

private:
  /// Refine a reference to an address.
  void RefineAddr(Inst &inst, Ref<Inst> addr);
  /// Refine a reference to an integer.
  void RefineInt(Inst &inst, Ref<Inst> addr);
  /// Refine a reference to a function.
  void RefineFunc(Inst &inst, Ref<Inst> addr);

private:
  void VisitMemoryLoadInst(MemoryLoadInst &i) override;
  void VisitMemoryStoreInst(MemoryStoreInst &i) override;
  void VisitSelectInst(SelectInst &i) override;
  void VisitSubInst(SubInst &i) override;
  void VisitAddInst(AddInst &i) override;
  void VisitAndInst(AndInst &i) override;
  void VisitOrInst(OrInst &i) override;
  void VisitXorInst(XorInst &i) override;
  void VisitCmpInst(CmpInst &i) override;
  void VisitMovInst(MovInst &i) override;
  void VisitPhiInst(PhiInst &phi) override;
  void VisitArgInst(ArgInst &arg) override;
  void VisitCallSite(CallSite &site) override;
  void VisitJumpCondInst(JumpCondInst &site) override;
  void VisitInst(Inst &i) override {}

private:
  /// Refine equality tests.
  void RefineEquality(
      Ref<Inst> lhs,
      Ref<Inst> rhs,
      Block *b,
      Block *bt,
      Block *bf
  );
  /// Refine inequalities.
  void RefineInequality(
      Ref<Inst> lhs,
      Ref<Inst> rhs,
      Block *b,
      Block *bt,
      Block *bf
  );
  /// Refine bit tests.
  void RefineAndOne(Ref<Inst> arg, Block *b, Block *bt, Block *bf);

private:
  /// Find the set of nodes where a value is live-in.
  std::pair<std::set<Block *>, std::set<Block *>>
  Liveness(
      Ref<Inst> ref,
      const llvm::SmallPtrSetImpl<const Block *> &defs
  );
  /// Define split points.
  void DefineSplits(
      DominatorCache &doms,
      Ref<Inst> ref,
      const std::unordered_map<const Block *, TaggedType> &splits
  );

private:
  /// Reference to the analysis.
  RegisterAnalysis &analysis_;
  /// Reference to target info.
  const Target *target_;
  /// Ban polymorphic arithmetic operators.
  bool banPolymorphism_;
  /// Function to optimise.
  Func &func_;
  /// Queue of instructions to simplify.
  std::queue<Inst *> queue_;
  /// Set of items in the queue.
  std::unordered_set<Inst *> inQueue_;
};

} // end namespace
