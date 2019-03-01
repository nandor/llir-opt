// This file if part of the genm-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include <llvm/Support/raw_ostream.h>

#include "core/atom.h"
#include "core/block.h"
#include "core/func.h"
#include "core/inst.h"
#include "core/insts_call.h"
#include "passes/global_data_elim/bitset.h"
#include "passes/global_data_elim/node.h"
#include "passes/global_data_elim/scc.h"
#include "passes/global_data_elim/solver.h"



// -----------------------------------------------------------------------------
ConstraintSolver::ConstraintSolver()
{
  extern_ = Root();
  Subset(Load(extern_), extern_);
}

// -----------------------------------------------------------------------------
SetNode *ConstraintSolver::Set()
{
  auto node = std::make_unique<SetNode>(nodes_.size());
  auto *ptr = node.get();
  nodes_.emplace_back(std::move(node));
  return ptr;
}

// -----------------------------------------------------------------------------
DerefNode *ConstraintSolver::Deref(SetNode *set)
{
  auto *contents = Root();
  auto node = std::make_unique<DerefNode>(set, contents, nodes_.size());
  auto *ptr = node.get();
  ptr->AddEdge(contents->Set());
  nodes_.emplace_back(std::move(node));
  return ptr;
}

// -----------------------------------------------------------------------------
Node *ConstraintSolver::Load(Node *ptr)
{
  auto *node = ptr->ToGraph();
  if (auto *set = node->AsSet()) {
    if (auto *deref = set->Deref()) {
      return deref;
    } else {
      return Deref(set);
    }
  }
  if (auto *deref = node->AsDeref()) {
    return Deref(deref->Contents());
  }
  return nullptr;
}

// -----------------------------------------------------------------------------
void ConstraintSolver::Subset(Node *from, Node *to)
{
  auto *nodeFrom = from->ToGraph();
  auto *nodeTo = to->ToGraph();
  if (auto *setFrom = nodeFrom->AsSet()) {
    if (auto *setTo = nodeTo->AsSet()) {
      setFrom->AddEdge(setTo);
    }
    if (auto *derefTo = nodeTo->AsDeref()) {
      setFrom->AddEdge(derefTo);
    }
  }
  if (auto *derefFrom = nodeFrom->AsDeref()) {
    if (auto *setTo = nodeTo->AsSet()) {
      derefFrom->AddEdge(setTo);
    }
    if (auto *derefTo = nodeTo->AsDeref()) {
      derefFrom->Contents()->AddEdge(derefTo);
    }
  }
}

// -----------------------------------------------------------------------------
RootNode *ConstraintSolver::Root()
{
  auto *set = Set();
  return Root(set);
}

// -----------------------------------------------------------------------------
HeapNode *ConstraintSolver::Heap()
{
  auto *set = Set();
  auto node = std::make_unique<HeapNode>(heap_.size(), set);
  auto *ptr = node.get();
  heap_.push_back(std::move(node));
  return ptr;
}

// -----------------------------------------------------------------------------
RootNode *ConstraintSolver::Root(Func *func)
{
  auto *set = Set();
  set->AddFunc(Map(func));
  return Root(set);
}

// -----------------------------------------------------------------------------
RootNode *ConstraintSolver::Root(Extern *ext)
{
  auto *set = Set();
  set->AddExtern(Map(ext));
  return Root(set);
}

// -----------------------------------------------------------------------------
RootNode *ConstraintSolver::Root(HeapNode *node)
{
  auto *set = Set();
  set->AddNode(node->GetID());
  return Root(set);
}

// -----------------------------------------------------------------------------
RootNode *ConstraintSolver::Root(SetNode *set)
{
  auto node = std::make_unique<RootNode>(set);
  auto *ptr = node.get();
  roots_.push_back(std::move(node));
  return ptr;
}

// -----------------------------------------------------------------------------
BitSet<Func *>::Item ConstraintSolver::Map(Func *func)
{
  auto it = funcToID_.emplace(func, idToFunc_.size());
  if (it.second) {
    idToFunc_.push_back(func);
  }
  return it.first->second;
}

// -----------------------------------------------------------------------------
BitSet<Extern *>::Item ConstraintSolver::Map(Extern *ext)
{
  auto it = extToID_.emplace(ext, idToExt_.size());
  if (it.second) {
    idToExt_.push_back(ext);
  }
  return it.first->second;
}

// -----------------------------------------------------------------------------
RootNode *ConstraintSolver::Anchor(Node *node)
{
  if (node) {
    if (auto *root = node->AsRoot()) {
      return root;
    } else {
      auto *graph = node->ToGraph();
      if (auto *set = graph->AsSet()) {
        for (auto &root : set->roots()) {
          return root;
        }
        return Root(set);
      }
      if (auto *deref = graph->AsDeref()) {
        return Root(deref->Contents());
      }
      return nullptr;
    }
  } else {
    return nullptr;
  }
}

// -----------------------------------------------------------------------------
Node *ConstraintSolver::Empty()
{
  return Set();
}

// -----------------------------------------------------------------------------
RootNode *ConstraintSolver::Chunk(Atom *atom, HeapNode *root)
{
  atoms_.emplace(atom, root);
  return root;
}

// -----------------------------------------------------------------------------
RootNode *ConstraintSolver::Call(
    const std::vector<Inst *> &context,
    Node *callee,
    const std::vector<Node *> &args)
{
  auto *ret = Root();

  std::vector<RootNode *> argsRoot;
  for (auto *node : args) {
    argsRoot.push_back(Anchor(node));
  }

  calls_.emplace_back(context, Anchor(callee), argsRoot, ret);
  return ret;
}

// -----------------------------------------------------------------------------
void ConstraintSolver::Solve()
{
  // Simplify the graph, coalescing strongly connected components.
  SCCSolver().Full(nodes_.begin(), nodes_.end()).Solve([this](auto &group) {
    if (group.size() <= 1) {
      return;
    }

    SetNode *united = nullptr;
    for (auto &node : group) {
      if (auto *set = node->AsSet()) {
        if (united) {
          set->Propagate(united);
          set->Replace(united);
          nodes_[set->GetID()] = nullptr;
        } else {
          united = set;
        }
      }
    }
  });

  // Find edges to propagate values along.
  std::set<std::pair<Node *, Node *>> visited;
  std::vector<SetNode *> setQueue;
  std::set<SetNode *> deleted;
  for (auto &node : nodes_) {
    if (node) {
      if (auto *set = node->AsSet()) {
        setQueue.push_back(set);
      }
    }
  }

  while (!setQueue.empty()) {
    auto *from = setQueue.back();
    setQueue.pop_back();

    if (deleted.count(from) != 0) {
      continue;
    }

    if (auto *deref = from->Deref()) {
      for (auto id : from->points_to_node()) {
        auto *root = heap_[id].get();
        auto *v = root->Set();
        for (auto *store : deref->set_ins()) {
          if (store->AddEdge(v)) {
            setQueue.push_back(store);
          }
        }
        for (auto *load : deref->set_outs()) {
          if (v->AddEdge(load)) {
            setQueue.push_back(v);
          }
        }
      }
    }

    bool collapse = false;
    for (auto *to : from->set_outs()) {
      if (from->Equals(to) && visited.insert(std::make_pair(from, to)).second) {
        collapse = true;
      }
      if (from->Propagate(to)) {
        setQueue.push_back(to);
      }
    }

    if (collapse) {
      SCCSolver().Single(from).Solve([&deleted, this](auto &group) {
        if (group.size() <= 1) {
          return;
        }

        SetNode *united = nullptr;
        for (auto &node : group) {
          if (auto *set = node->AsSet()) {
            if (united) {
              set->Propagate(united);
              set->Replace(united);
              nodes_[set->GetID()] = nullptr;
              deleted.insert(set);
            } else {
              united = set;
            }
          }
        }
      });
    }
  }
}

// -----------------------------------------------------------------------------
std::vector<std::pair<std::vector<Inst *>, Func *>> ConstraintSolver::Expand()
{
  Solve();

  std::vector<std::pair<std::vector<Inst *>, Func *>> callees;
  for (auto &call : calls_) {
    for (auto id : call.Callee->Set()->points_to_func()) {
      auto *func = idToFunc_[id];

      // Only expand each call site once.
      if (!call.Expanded.insert(func).second) {
        continue;
      }

      // Call to be expanded, with context.
      callees.emplace_back(call.Context, func);

      // Connect arguments and return value.
      auto &funcSet = this->Lookup(call.Context, func);
      for (unsigned i = 0; i < call.Args.size(); ++i) {
        if (auto *arg = call.Args[i]) {
          if (i >= funcSet.Args.size()) {
            if (func->IsVarArg()) {
              Subset(arg, funcSet.VA);
            }
          } else {
            Subset(arg, funcSet.Args[i]);
          }
        }
      }
      Subset(funcSet.Return, call.Return);
    }

    for (auto id : call.Callee->Set()->points_to_ext()) {
      assert(!"not implemented");
    }
  }

  return callees;
}

// -----------------------------------------------------------------------------
RootNode *ConstraintSolver::Lookup(Global *g)
{
  auto it = globals_.emplace(g, nullptr);
  if (it.second) {
    switch (g->GetKind()) {
      case Global::Kind::SYMBOL: {
        break;
      }
      case Global::Kind::EXTERN: {
        it.first->second = Root(static_cast<Extern *>(g));
        break;
      }
      case Global::Kind::FUNC: {
        it.first->second = Root(static_cast<Func *>(g));
        break;
      }
      case Global::Kind::BLOCK: {
        break;
      }
      case Global::Kind::ATOM: {
        it.first->second = Root(atoms_[static_cast<Atom *>(g)]);
        break;
      }
    }
  }
  return it.first->second;

}

// -----------------------------------------------------------------------------
ConstraintSolver::FuncSet &ConstraintSolver::Lookup(
    const std::vector<Inst *> &calls,
    Func *func)
{
  auto key = func;
  auto it = funcs_.emplace(key, nullptr);
  if (it.second) {
    it.first->second = std::make_unique<FuncSet>();
    auto f = it.first->second.get();
    f->Return = Root();
    f->VA = Root();
    f->Frame = Root();
    for (auto &arg : func->params()) {
      f->Args.push_back(Root());
    }
    f->Expanded = false;
  }
  return *it.first->second;
}
