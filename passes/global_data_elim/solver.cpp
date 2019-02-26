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
template<typename T, typename... Args>
T *ConstraintSolver::Make(Args... args)
{
  auto node = std::make_unique<T>(args...);
  auto *ptr = node.get();
  pending_.emplace_back(std::move(node));
  return ptr;
}

// -----------------------------------------------------------------------------
Node *ConstraintSolver::Load(Node *ptr)
{
  auto *node = ptr->ToGraph();
  if (auto *deref = node->Deref()) {
    return deref;
  } else {
    return Make<DerefNode>(node);
  }
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
      auto *middle = Make<SetNode>();
      derefFrom->AddEdge(middle);
      middle->AddEdge(derefTo);
    }
  }
}

// -----------------------------------------------------------------------------
RootNode *ConstraintSolver::Root()
{
  auto *set = Make<SetNode>();
  return Root(set);
}


// -----------------------------------------------------------------------------
RootNode *ConstraintSolver::Root(Func *func)
{
  auto *set = Make<SetNode>();
  set->AddFunc(func);
  return Root(set);
}

// -----------------------------------------------------------------------------
RootNode *ConstraintSolver::Root(Extern *ext)
{
  auto *set = Make<SetNode>();
  set->AddExtern(ext);
  return Root(set);
}

// -----------------------------------------------------------------------------
RootNode *ConstraintSolver::Root(RootNode *node)
{
  auto *set = Make<SetNode>();
  set->AddNode(node);
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
        auto *middle = Make<SetNode>();
        deref->AddEdge(middle);
        return Root(middle);
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
  return Make<SetNode>();
}

// -----------------------------------------------------------------------------
RootNode *ConstraintSolver::Chunk(Atom *atom, RootNode *root)
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
void ConstraintSolver::Progress()
{
  // Transfer all relevant nodes to the pending list.
  for (auto &node : pending_) {
    // TODO: simplify the pending nodes.
    nodes_.emplace_back(std::move(node));
  }

  pending_.clear();
}

// -----------------------------------------------------------------------------
void ConstraintSolver::Collapse(SetNode *node)
{
  SCCSolver().Single(node).Solve([](auto &scc) {
    if (scc.size() != 1) {
      for (auto *node : scc) {
        llvm::errs() << node << " ";
      }
      llvm::errs() << "\n";
    }
  });
}

// -----------------------------------------------------------------------------
void ConstraintSolver::Solve()
{
  // Simplify the graph, coalescing strongly connected components.
  std::set<GraphNode *> toDelete;
  SCCSolver().Full(nodes_.begin(), nodes_.end()).Solve([&](const auto &group) {
    if (group.size() <= 1) {
      return;
    }

    SetNode *united = nullptr;
    for (auto &node : group) {
      llvm::errs() << node << " ";
      if (auto *set = node->AsSet()) {
        if (united) {
          set->Propagate(united);
          set->Replace(united);
          toDelete.insert(set);
        } else {
          united = set;
        }
      }
    }
    llvm::errs() << "\n";
  });

  // Remove the deleted nodes.
  for (auto &node : nodes_) {
    if (toDelete.count(node.get()) != 0) {
      node = nullptr;
    }
  }

  llvm::errs() << "Node count: " << nodes_.size() << "\n";

  // Find edges to propagate values along.
  std::set<std::pair<Node *, Node *>> visited;
  std::vector<SetNode *> setQueue;
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

    if (auto *deref = from->Deref()) {
      for (auto *root : from->points_to_node()) {
        auto *v = root->Set();
        for (auto *store : deref->set_ins()) {
          if (store->AddEdge(v)) {
            setQueue.push_back(store);
          }
        }
        for (auto *load : deref->set_outs()) {
          if (v->AddEdge(load)) {
            setQueue.push_back(load);
          }
        }
      }
    }

    for (auto *to : from->set_outs()) {
      if (from->Equals(to) && visited.insert(std::make_pair(from, to)).second) {
        Collapse(from);
      }
      if (from->Propagate(to)) {
        setQueue.push_back(to);
      }
    }
  }
}

// -----------------------------------------------------------------------------
std::vector<std::pair<std::vector<Inst *>, Func *>> ConstraintSolver::Expand()
{
  Solve();

  std::vector<std::pair<std::vector<Inst *>, Func *>> callees;
  for (auto &call : calls_) {
    for (auto *func : call.Callee->Set()->points_to_func()) {
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

      // Simplify the constraints that were added.
      Progress();
    }

    for (auto *ext : call.Callee->Set()->points_to_node()) {
      assert(!"not implemented");
    }
  }

  return callees;
}

// -----------------------------------------------------------------------------
Node *ConstraintSolver::Lookup(Global *g)
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
