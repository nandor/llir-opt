// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#pragma once

#include <functional>
#include <unordered_map>

#include "core/analysis/live_variables.h"
#include "core/adt/cache.h"
#include "core/adt/id.h"
#include "core/adt/queue.h"
#include "core/inst.h"
#include "core/insts.h"
#include "passes/local_const/graph.h"


/// Forward declarations.
class LCContext;



/**
 * Graph builder which deduplicates nodes.
 */
class GraphBuilder final {
public:
  GraphBuilder(LCContext &context, Func &func, Queue<LCSet> &queue);
  ~GraphBuilder();

  void BuildCall(Inst &inst);
  void BuildReturn(ReturnInst &inst);
  void BuildFrame(FrameInst &inst);
  void BuildArg(ArgInst &inst);
  void BuildMov(MovInst &inst);
  void BuildLoad(LoadInst &inst);
  void BuildStore(StoreInst &inst);
  void BuildFlow(BinaryInst &inst);
  void BuildExtern(Inst &inst, Global *global);
  void BuildMove(Inst &inst, Inst *arg);
  void BuildPhi(PhiInst &inst);
  void BuildAdd(AddInst &inst);
  void BuildSub(SubInst &inst);
  void BuildAlloca(AllocaInst &inst);
  void BuildVAStart(VAStartInst &inst);
  void BuildSelect(SelectInst &inst);
  void BuildX86_Xchg(X86_XchgInst &inst);
  void BuildX86_CmpXchg(X86_CmpXchgInst &inst);

private:
  /// Builder for individual calls.
  LCSet *BuildCall(CallSite &call);
  /// Finishes a PHI node after all incoming args were built.
  void FixupPhi(PhiInst &inst);
  /// Allocation site, generating a pointer to an element.
  LCSet *Alloc(uint64_t index, const std::optional<uint64_t> &size);
  /// Generates a store edge.
  void Store(LCSet *from, LCSet *to);
  /// Cached node generation.
  LCSet *Return(LCSet *set);
  LCSet *Load(LCSet *from);
  LCSet *Offset(LCSet *set, int64_t offset);
  LCSet *Union(LCSet *a, LCSet *b);
  LCSet *Range(LCSet *r);

  /// Returns a set for an extern.
  LCSet *GetGlobal(const Global *global);

private:
  /// Context for the function.
  LCContext &context_;
  /// Reference to the function.
  Func &func_;
  /// Graph being built.
  LCGraph &graph_;
  /// Initial queue for propagation.
  Queue<LCSet> &queue_;
  /// Cached empty set.
  ID<LCSet> empty_;
  /// Extern allocation.
  LCAlloc *externAlloc_;
  /// Root allocation.
  LCAlloc *rootAlloc_;
  /// Set with a frame pointer to an object of unknown size for allocas.
  std::optional<ID<LCSet>> alloca_;
  /// PHIs to fix up.
  std::vector<PhiInst *> phis_;
  /// Live variable analysis results, if required.
  std::unique_ptr<LiveVariables> lva_;
  /// Cached frame nodes.
  Cache<ID<LCSet>, unsigned> frameCache_;
  /// Cached load nodes.
  Cache<ID<LCSet>, ID<LCSet>> loadCache_;
  /// Cached offset nodes.
  Cache<ID<LCSet>, ID<LCSet>, int64_t> offsetCache_;
  /// Cached union nodes.
  Cache<ID<LCSet>, ID<LCSet>, ID<LCSet>> unionCache_;
  //// Cached range nodes.
  Cache<ID<LCSet>, ID<LCSet>> rangeCache_;
};
