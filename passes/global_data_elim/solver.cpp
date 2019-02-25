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
#include "passes/global_data_elim/solver.h"



// -----------------------------------------------------------------------------
ConstraintSolver::ConstraintSolver()
{
  extern_ = Root();
  Subset(extern_->Deref(), extern_);
}

// -----------------------------------------------------------------------------
Node *ConstraintSolver::Load(Node *ptr)
{
  return ptr->Deref();
}

// -----------------------------------------------------------------------------
void ConstraintSolver::Subset(Node *from, Node *to)
{
  from->AddEdge(to);
}

// -----------------------------------------------------------------------------
RootNode *ConstraintSolver::Root()
{
  auto *n = new RootNode();
  roots_.push_back(n);
  return n;
}

// -----------------------------------------------------------------------------
RootNode *ConstraintSolver::Root(uint64_t item)
{
  auto *n = new RootNode(item);
  roots_.push_back(n);
  return n;
}

// -----------------------------------------------------------------------------
RootNode *ConstraintSolver::Root(RootNode *node)
{
  return Root(Map(node));
}

// -----------------------------------------------------------------------------
RootNode *ConstraintSolver::Root(Func *func)
{
  return Root(Map(func));
}

// -----------------------------------------------------------------------------
RootNode *ConstraintSolver::Root(Extern *ext)
{
  return Root(Map(ext));
}

// -----------------------------------------------------------------------------
Node *ConstraintSolver::Empty()
{
  return new SetNode();
}

// -----------------------------------------------------------------------------
RootNode *ConstraintSolver::Chunk(Atom *atom, RootNode *root)
{
  globals_.emplace(atom, root);
  return root;
}

// -----------------------------------------------------------------------------
uint64_t ConstraintSolver::Map(Func *func)
{
  auto it = funcIDs_.emplace(func, funcIDs_.size());
  return (0ull << 62ull) | static_cast<uint64_t>(it.first->second);
}

// -----------------------------------------------------------------------------
uint64_t ConstraintSolver::Map(Extern *ext)
{
  auto it = extIDs_.emplace(ext, extIDs_.size());
  return (1ull << 62ull) | static_cast<uint64_t>(it.first->second);
}

// -----------------------------------------------------------------------------
uint64_t ConstraintSolver::Map(RootNode *node)
{
  auto it = rootIDs_.emplace(node, rootIDs_.size());
  return (2ull << 62ull) | static_cast<uint64_t>(it.first->second);
}

// -----------------------------------------------------------------------------
void ConstraintSolver::Progress()
{
}

// -----------------------------------------------------------------------------
std::vector<std::pair<std::vector<Inst *>, Func *>> ConstraintSolver::Expand()
{
  assert(!"not implemented");
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
