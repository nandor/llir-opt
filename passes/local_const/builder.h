// This file if part of the genm-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#pragma once

#include <functional>
#include <unordered_map>

#include "core/adt/cache.h"
#include "core/adt/id.h"
#include "core/adt/queue.h"
#include "core/inst.h"
#include "core/insts.h"
#include "passes/local_const/graph.h"



/**
 * Graph builder which deduplicates nodes.
 */
class GraphBuilder final {
public:
  using NodeMap = std::unordered_map<const Inst *, ID<LCSet>>;

  GraphBuilder(
      LCGraph &graph,
      Queue<LCSet> &queue,
      NodeMap &nodes,
      ID<LCSet> ext
  );
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
  void BuildXchg(ExchangeInst &inst);
  void BuildVAStart(VAStartInst &inst);
  void BuildSelect(SelectInst &inst);

private:
  /// Maps an instruction to a specific node.
  LCSet *Map(const Inst *inst, LCSet *node)
  {
    ID<LCSet> id = node->GetID();
    nodes_.emplace(inst, id).first->second = id;
    return node;
  }

  /// Returns an instruction.
  LCSet *Get(const Inst *inst)
  {
    if (auto it = nodes_.find(inst); it != nodes_.end()) {
      return graph_.Find(it->second);
    }
    return nullptr;
  }

  /// Builder for individual calls.
  template<typename T> LCSet *BuildCall(CallSite<T> &call);
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

private:
  /// Graph being built.
  LCGraph &graph_;
  /// Initial queue for propagation.
  Queue<LCSet> &queue_;
  /// Mapping from instructions to nodes.
  NodeMap &nodes_;
  /// The identifier of the frame node.
  LCAlloc *frame_;
  /// Cached empty set.
  ID<LCSet> empty_;
  /// Extern allocation.
  ID<LCAlloc> externAlloc_;
  /// Extern node.
  ID<LCSet> extern_;
  /// PHIs to fix up.
  std::vector<PhiInst *> phis_;
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
