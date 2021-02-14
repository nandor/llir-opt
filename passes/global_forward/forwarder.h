// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#pragma once

#include "core/adt/bitset.h"
#include "core/prog.h"
#include "core/func.h"
#include "core/dag.h"
#include "core/inst_visitor.h"
#include "passes/global_forward/nodes.h"

class MovInst;



/**
 * Implementation of the global value to code forwarder.
 */
class GlobalForwarder final {
public:
  /// Initialise the analysis.
  GlobalForwarder(Prog &prog, Func &entry);

  /// Simplify loads and build the graph for the reverse transformations.
  bool Forward();
  /// Hoist stores.
  bool Reverse();

private:
  /// Evaluation state of a function.
  struct FuncState {
    /// Summarised function.
    DAGFunc &DAG;
    /// Current active node.
    unsigned Active;
    /// ID of the node to evaluate accurately.
    unsigned Accurate;
    /// Enumeration of node states.
    std::unordered_map<unsigned, std::unique_ptr<NodeState>> States;

    FuncState(DAGFunc &dag)
      : DAG(dag)
      , Active(dag.rbegin()->Index)
      , Accurate(Active)
    {
    }

    NodeState &GetState(unsigned index)
    {
      auto it = States.emplace(index, nullptr);
      if (it.second) {
        it.first->second.reset(new NodeState());
      }
      return *it.first->second;
    }
  };

  /// Visitor for accurate evaluation.
  class Approximator : public InstVisitor<void> {
  public:
    Approximator(GlobalForwarder &state) : state_(state) {}

    void VisitInst(Inst &inst) override { }

    void VisitMovInst(MovInst &mov) override;
    void VisitMemoryStoreInst(MemoryStoreInst &store) override;
    void VisitMemoryLoadInst(MemoryLoadInst &load) override;
    void VisitCallSite(CallSite &site) override;
    void VisitTrapInst(TrapInst &) override { }
    void VisitRaiseInst(RaiseInst &raise) override { Raises = true; }

  public:
    /// Flag to set if any node raises.
    bool Raises = false;
    /// Flag to indicate if indirect calls are present.
    bool Indirect = false;
    /// Set of referenced functions.
    BitSet<Func> Funcs;
    /// Set of escaped symbols.
    BitSet<Object> Escaped;
    /// Set of loaded symbols.
    BitSet<Object> Loaded;
    /// Set of stored symbols.
    BitSet<Object> Stored;

  private:
    /// Reference to the transformation.
    GlobalForwarder &state_;
  };

  /// Accurate evaluator which can simplify nodes.
  class Simplifier final : public InstVisitor<bool> {
  public:
    Simplifier(
        GlobalForwarder &state,
        NodeState &node,
        ReverseNodeState &reverse)
      : state_(state)
      , node_(node)
      , reverse_(reverse)
    {
    }

    bool VisitInst(Inst &inst) override { return false; }

    bool VisitAddInst(AddInst &add) override;
    bool VisitMovInst(MovInst &mov) override;
    bool VisitMemoryStoreInst(MemoryStoreInst &store) override;
    bool VisitMemoryLoadInst(MemoryLoadInst &load) override;
    bool VisitMemoryExchangeInst(MemoryExchangeInst &xchg) override;

    bool VisitTerminatorInst(TerminatorInst &) override
    {
      llvm_unreachable("cannot evaluate terminator");
    }

  protected:
    /// Reference to the transformation.
    GlobalForwarder &state_;
    /// Reference to the node containing the instruction.
    NodeState &node_;
    /// Reverse node.
    ReverseNodeState &reverse_;
  };


private:
  /// Approximate the effects of a mov.
  void Escape(BitSet<Func> &funcs, BitSet<Object> &escaped, MovInst &mov);
  /// Approximate the effects of a call.
  void Indirect(
      BitSet<Func> &funcs,
      BitSet<Object> &escaped,
      BitSet<Object> &stored,
      BitSet<Object> &loaded,
      bool &raises
  );
  /// Approximate the effects of a raise.
  void Raise(NodeState &node, ReverseNodeState &reverse);

  /// Return the ID of a function.
  ID<Func> GetFuncID(Func &func)
  {
    auto it = funcToID_.emplace(&func, funcs_.size());
    if (it.second) {
      funcs_.emplace_back(std::make_unique<FuncClosure>());
    }
    return it.first->second;
  }

  /// Return the ID of an object.
  ID<Object> GetObjectID(Object *object)
  {
    auto it = objectToID_.find(object);
    assert(it != objectToID_.end() && "missing object");
    return it->second;
  }

  /// Helper to get the DAG for a function.
  DAGFunc &GetDAG(Func &func)
  {
    auto &dag = funcs_[GetFuncID(func)]->DAG;
    if (!dag) {
      dag.reset(new DAGFunc(func));
    }
    return *dag;
  }

  /// Return a reverse node.
  ReverseNodeState &GetReverseNode(Func &func, unsigned index)
  {
    std::pair<Func *, unsigned> key(&func, index);
    auto it = reverse_.emplace(key, nullptr);
    if (it.second) {
      it.first->second.reset(new ReverseNodeState(*(GetDAG(func)[index])));
    }
    return *it.first->second;
  }

private:
  /// Analysed program.
  Prog &prog_;
  /// Entry point.
  Func &entry_;

  /// Object to ID.
  std::unordered_map<Object *, ID<Object>> objectToID_;
  /// Mapping from objects to their closures.
  std::vector<std::unique_ptr<ObjectClosure>> objects_;
  /// Mapping from object IDs to objects.
  std::vector<Object *> idToObject_;

  /// Function to ID.
  std::unordered_map<Func *, ID<Func>> funcToID_;
  /// Mapping from functions to their closures.
  std::vector<std::unique_ptr<FuncClosure>> funcs_;

  /// Set of reverse nodes.
  std::unordered_map
    < std::pair<Func *, unsigned>
    , std::unique_ptr<ReverseNodeState>
    > reverse_;

  /// Evaluation stack.
  std::vector<FuncState> stack_;
};
