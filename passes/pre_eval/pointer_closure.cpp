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
  nodes_.emplace_back(*this);

  for (auto &object : ctx.objects()) {
    Build(GetNode(object.GetID()), object);
  }

  for (auto it = llvm::scc_begin(this); !it.isAtEnd(); ++it) {
    std::vector<Node *> nodes(*it);
    Node *base = nodes[0];
    for (unsigned i = 1; i < nodes.size(); ++i) {
      auto *node = nodes[i];
      for (auto id : node->nodes_) {
        base->nodes_.Union(nodes_[id].nodes_);
        for (auto *f : nodes_[id].funcs_) {
          base->funcs_.insert(f);
        }
      }
      base->nodes_.Union(node->nodes_);
      for (auto *f : node->funcs_) {
        base->funcs_.insert(f);
      }
    }
    for (unsigned i = 1; i < nodes.size(); ++i) {
      auto *node = nodes[i];
      node->nodes_ = base->nodes_;
      node->funcs_ = base->funcs_;
    }
  }
}

// -----------------------------------------------------------------------------
void PointerClosure::Add(const SymbolicValue &value)
{
  auto addNode = [&, this] (auto id)
  {
    auto &n = nodes_[id];
    for (auto *f : n.funcs_) {
      funcs_.insert(f);
    }
    closure_.Union(n.nodes_);
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
          llvm::errs() << "TODO: " << ext->getName() << "\n";
          continue;
        }
        case SymbolicAddress::Kind::EXTERN_RANGE: {
          auto &ext = addr.AsExtern().Symbol;
          llvm::errs() << "TODO: " << ext->getName() << "\n";
          continue;
        }
        case SymbolicAddress::Kind::FUNC: {
          funcs_.insert(addr.AsFunc().F);
          continue;
        }
        case SymbolicAddress::Kind::BLOCK: {
          auto &block = addr.AsBlock().B;
          llvm::errs() << "TODO: " << block->getName() << "\n";
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
void PointerClosure::Add(Object *g)
{
  auto &n = nodes_[GetNode(heap_.Data(g))];
  for (auto *f : n.funcs_) {
    funcs_.insert(f);
  }
  closure_.Union(n.nodes_);
}

// -----------------------------------------------------------------------------
void PointerClosure::Add(Func *f)
{
  funcs_.insert(f);
}

// -----------------------------------------------------------------------------
SymbolicValue PointerClosure::Build()
{
  SymbolicPointer ptr;
  for (Func *f : funcs_) {
    ptr.Add(f);
  }
  for (auto frame : stacks_) {
    ptr.Add(frame);
  }
  for (auto id : closure_) {
    ptr.Add(id);
  }
  return SymbolicValue::Value(ptr);
}

// -----------------------------------------------------------------------------
ID<PointerClosure::Node> PointerClosure::GetNode(ID<SymbolicObject> id)
{
  auto it = objectToNode_.emplace(id, nodes_.size());
  if (it.second) {
    nodes_.emplace_back(*this);
    nodes_[0].nodes_.Insert(it.first->second);
  }
  return it.first->second;
}

// -----------------------------------------------------------------------------
void PointerClosure::Build(ID<Node> id, SymbolicObject &object)
{
  for (const auto &value : object) {
    if (auto ptr = value.AsPointer()) {
      for (auto &addr : *ptr) {
        switch (addr.GetKind()) {
          case SymbolicAddress::Kind::OBJECT: {
            nodes_[id].nodes_.Insert(GetNode(addr.AsObject().Object));
            continue;
          }
          case SymbolicAddress::Kind::OBJECT_RANGE: {
            nodes_[id].nodes_.Insert(GetNode(addr.AsObjectRange().Object));
            continue;
          }
          case SymbolicAddress::Kind::EXTERN: {
            auto &ext = addr.AsExtern().Symbol;
            llvm::errs() << "TODO: " << ext->getName() << "\n";
            continue;
          }
          case SymbolicAddress::Kind::EXTERN_RANGE: {
            auto &ext = addr.AsExternRange().Symbol;
            llvm::errs() << "TODO: " << ext->getName() << "\n";
            continue;
          }
          case SymbolicAddress::Kind::FUNC: {
            nodes_[id].funcs_.insert(addr.AsFunc().F);
            continue;
          }
          case SymbolicAddress::Kind::BLOCK: {
            auto *block = addr.AsBlock().B;
            llvm::errs() << "TODO: " << block->getName() << "\n";
            continue;
          }
          case SymbolicAddress::Kind::STACK: {
            nodes_[id].stacks_.insert(addr.AsStack().Frame);
            continue;
          }
        }
        llvm_unreachable("invalid address kind");
      }
    }
  }
}
