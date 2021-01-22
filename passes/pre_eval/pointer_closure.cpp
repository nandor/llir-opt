// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include <llvm/ADT/SCCIterator.h>

#include "core/atom.h"
#include "core/block.h"
#include "core/object.h"
#include "core/extern.h"
#include "passes/pre_eval/pointer_closure.h"
#include "passes/pre_eval/symbolic_context.h"
#include "passes/pre_eval/symbolic_heap.h"



// -----------------------------------------------------------------------------
template <>
struct llvm::GraphTraits<PointerClosure::Node *> {
  using NodeRef = PointerClosure::Node *;
  using ChildIteratorType = PointerClosure::Node::node_iterator;

  static ChildIteratorType child_begin(NodeRef n) { return n->nodes_begin(); }
  static ChildIteratorType child_end(NodeRef n) { return n->nodes_end(); }
};

// -----------------------------------------------------------------------------
template <>
struct llvm::GraphTraits<PointerClosure *> : public llvm::GraphTraits<PointerClosure::Node *> {
  static NodeRef getEntryNode(PointerClosure *g) { return g->GetRoot(); }
};

// -----------------------------------------------------------------------------
PointerClosure::PointerClosure(SymbolicHeap &heap, SymbolicContext &ctx)
  : heap_(heap)
  , ctx_(ctx)
{
  nodes_.Emplace(*this);

  for (auto &object : ctx.objects()) {
    Build(GetNode(object.GetID()), object);
    objects_.insert(&object);
  }
  Compact();
}

// -----------------------------------------------------------------------------
void PointerClosure::Add(const SymbolicValue &value)
{
  auto addNode = [&, this] (auto id)
  {
    const auto *n = nodes_.Map(id);
    for (auto *f : n->funcs_) {
      funcs_.insert(f);
    }
    for (auto stack : n->stacks_) {
      stacks_.insert(stack);
    }
    closure_.Union(n->self_);
    closure_.Union(n->refs_);
  };

  if (auto ptr = value.AsPointer()) {
    for (auto &addr : *ptr) {
      switch (addr.GetKind()) {
        case SymbolicAddress::Kind::OBJECT: {
          auto &h = addr.AsObject();
          addNode(GetNode(h.Object));
          continue;
        }
        case SymbolicAddress::Kind::OBJECT_RANGE: {
          auto &h = addr.AsObjectRange();
          addNode(GetNode(h.Object));
          continue;
        }
        case SymbolicAddress::Kind::EXTERN: {
          auto &ext = addr.AsExtern().Symbol;
          //llvm::errs() << "TODO: " << ext->getName() << "\n";
          continue;
        }
        case SymbolicAddress::Kind::EXTERN_RANGE: {
          auto &ext = addr.AsExtern().Symbol;
          //llvm::errs() << "TODO: " << ext->getName() << "\n";
          continue;
        }
        case SymbolicAddress::Kind::FUNC: {
          funcs_.insert(addr.AsFunc().F);
          continue;
        }
        case SymbolicAddress::Kind::BLOCK: {
          auto &block = addr.AsBlock().B;
          //llvm::errs() << "TODO: " << block->getName() << "\n";
          continue;
        }
        case SymbolicAddress::Kind::STACK: {
          stacks_.insert(addr.AsStack().Frame);
          continue;
        }
      }
      llvm_unreachable("invalid address kind");
    }
  }
}

// -----------------------------------------------------------------------------
void PointerClosure::AddRead(Object *g)
{
  const auto *n = nodes_.Map(GetNode(g));
  for (auto *f : n->funcs_) {
    funcs_.insert(f);
  }
  closure_.Union(n->refs_);
  closure_.Union(n->self_);
}

// -----------------------------------------------------------------------------
void PointerClosure::AddWritten(Object *g)
{
  const auto *n = nodes_.Map(GetNode(g));
  for (auto *f : n->funcs_) {
    funcs_.insert(f);
  }
  closure_.Union(n->refs_);
  closure_.Union(n->self_);
}

// -----------------------------------------------------------------------------
void PointerClosure::AddEscaped(Object *g)
{
  const auto *n = nodes_.Map(GetNode(g));
  for (auto *f : n->funcs_) {
    funcs_.insert(f);
  }
  closure_.Union(n->refs_);
  closure_.Union(n->self_);
}

// -----------------------------------------------------------------------------
void PointerClosure::Add(Func *f)
{
  funcs_.insert(f);
}

// -----------------------------------------------------------------------------
SymbolicValue PointerClosure::BuildTainted()
{
  if (funcs_.empty() && stacks_.empty() && closure_.Empty()) {
    return SymbolicValue::Scalar();
  }
  auto ptr = std::make_shared<SymbolicPointer>();
  for (Func *f : funcs_) {
    ptr->Add(f);
  }
  for (auto frame : stacks_) {
    ptr->Add(frame);
  }
  for (ID<SymbolicObject> id : closure_) {
    ptr->Add(id);
  }
  return SymbolicValue::Value(ptr);
}

// -----------------------------------------------------------------------------
SymbolicValue PointerClosure::BuildTaint()
{
  if (funcs_.empty() && stacks_.empty() && closure_.Empty()) {
    return SymbolicValue::Scalar();
  }
  auto ptr = std::make_shared<SymbolicPointer>();
  for (Func *f : funcs_) {
    ptr->Add(f);
  }
  for (auto frame : stacks_) {
    ptr->Add(frame);
  }
  for (ID<SymbolicObject> id : closure_) {
    ptr->Add(id);
  }
  return SymbolicValue::Value(ptr);
}

// -----------------------------------------------------------------------------
ID<PointerClosure::Node> PointerClosure::GetNode(ID<SymbolicObject> id)
{
  auto it = objectToNode_.emplace(id, 0);
  if (it.second) {
    auto nodeID = nodes_.Emplace(*this);
    it.first->second = nodeID;
    nodes_.Map(nodeID)->self_.Insert(id);
    nodes_.Map(0)->nodes_.Insert(nodeID);
  }
  return it.first->second;
}

// -----------------------------------------------------------------------------
ID<PointerClosure::Node> PointerClosure::GetNode(Object *object)
{
  auto &repr = ctx_.GetObject(object);
  if (objects_.insert(&repr).second) {
    Compact();
  }
  return GetNode(repr.GetID());
}

// -----------------------------------------------------------------------------
void PointerClosure::Build(ID<Node> id, SymbolicObject &object)
{
  for (const auto &value : object) {
    if (auto ptr = value.AsPointer()) {
      for (auto &addr : *ptr) {
        switch (addr.GetKind()) {
          case SymbolicAddress::Kind::OBJECT: {
            auto objectID = GetNode(addr.AsObject().Object);
            nodes_.Map(id)->nodes_.Insert(objectID);
            continue;
          }
          case SymbolicAddress::Kind::OBJECT_RANGE: {
            auto objectID = GetNode(addr.AsObjectRange().Object);
            nodes_.Map(id)->nodes_.Insert(objectID);
            continue;
          }
          case SymbolicAddress::Kind::EXTERN: {
            auto &ext = addr.AsExtern().Symbol;
            continue;
          }
          case SymbolicAddress::Kind::EXTERN_RANGE: {
            auto &ext = addr.AsExternRange().Symbol;
            continue;
          }
          case SymbolicAddress::Kind::FUNC: {
            nodes_.Map(id)->funcs_.insert(addr.AsFunc().F);
            continue;
          }
          case SymbolicAddress::Kind::BLOCK: {
            auto *block = addr.AsBlock().B;
            continue;
          }
          case SymbolicAddress::Kind::STACK: {
            nodes_.Map(id)->stacks_.Insert(addr.AsStack().Frame);
            continue;
          }
        }
        llvm_unreachable("invalid address kind");
      }
    }
  }
}

void PointerClosure::Compact()
{
  std::vector<std::vector<ID<Node>>> nodes;
  for (auto it = llvm::scc_begin(this); !it.isAtEnd(); ++it) {
    auto &scc = nodes.emplace_back();
    for (Node *node : *it) {
      scc.push_back(node->id_);
    }
  }

  for (const auto &scc : nodes) {
    for (unsigned i = 1, n = scc.size(); i < n; ++i) {
      nodes_.Union(scc[0], scc[i]);
    }
    auto *node = nodes_.Map(scc[0]);
    for (ID<Node> refID : node->nodes_) {
      if (auto *ref = nodes_.Get(refID)) {
        node->refs_.Union(ref->self_);
        node->refs_.Union(ref->refs_);
        node->stacks_.Union(ref->stacks_);
        for (auto *f : ref->funcs_) {
          node->funcs_.insert(f);
        }
      }
    }
  }

  auto *root = nodes_.Get(0);
  root->nodes_.Clear();
}
