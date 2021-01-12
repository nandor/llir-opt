// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include <llvm/ADT/SCCIterator.h>

#include "core/atom.h"
#include "core/block.h"
#include "core/object.h"
#include "core/extern.h"
#include "passes/pre_eval/heap_graph.h"
#include "passes/pre_eval/symbolic_context.h"



// -----------------------------------------------------------------------------
template <>
struct llvm::GraphTraits<HeapGraph::Node *> {
  using NodeRef = HeapGraph::Node *;
  using ChildIteratorType = HeapGraph::Node::node_iterator;

  static ChildIteratorType child_begin(NodeRef n) { return n->nodes_begin(); }
  static ChildIteratorType child_end(NodeRef n) { return n->nodes_end(); }
};

// -----------------------------------------------------------------------------
template <>
struct llvm::GraphTraits<HeapGraph *> : public llvm::GraphTraits<HeapGraph::Node *> {
  static NodeRef getEntryNode(HeapGraph *g) { return g->GetRoot(); }
};


// -----------------------------------------------------------------------------
HeapGraph::HeapGraph(SymbolicContext &ctx)
  : ctx_(ctx)
{
  nodes_.emplace_back(*this);

  for (auto &object : ctx.objects()) {
    Build(GetNode(object.GetID()), object);
  }
  for (auto &object : ctx.allocs()) {
    Build(GetNode(object.GetID()), object);
  }
  for (auto &frame : ctx.frames()) {
    for (auto &object : frame.objects()) {
      Build(GetNode(object.GetID()), object);
    }
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
void HeapGraph::Extract(
    const SymbolicValue &value,
    std::set<Func *> &funcs,
    std::set<unsigned> &stacks,
    BitSet<Node> &nodes)
{
  auto addNode = [&, this] (auto id)
  {
    auto &n = nodes_[id];
    for (auto *f : n.funcs_) {
      funcs.insert(f);
    }
    nodes.Union(n.nodes_);
  };

  if (auto ptr = value.AsPointer()) {
    for (auto &addr : *ptr) {
      switch (addr.GetKind()) {
        case SymbolicAddress::Kind::ATOM: {
          auto &h = addr.AsAtom();
          addNode(GetNode(h.Symbol->getParent()));
          continue;
        }
        case SymbolicAddress::Kind::ATOM_RANGE: {
          auto &h = addr.AsAtomRange();
          addNode(GetNode(h.Symbol->getParent()));
          continue;
        }
        case SymbolicAddress::Kind::FRAME: {
          auto &h = addr.AsFrame();
          addNode(GetNode({ h.Frame, h.Object }));
          continue;
        }
        case SymbolicAddress::Kind::FRAME_RANGE: {
          auto &h = addr.AsFrameRange();
          addNode(GetNode({ h.Frame, h.Object }));
          continue;
        }
        case SymbolicAddress::Kind::HEAP: {
          auto &h = addr.AsHeap();
          addNode(GetNode({ h.Frame, h.Alloc }));
          continue;
        }
        case SymbolicAddress::Kind::HEAP_RANGE: {
          auto &h = addr.AsHeapRange();
          addNode(GetNode({ h.Frame, h.Alloc }));
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
          funcs.insert(addr.AsFunc().Fn);
          continue;
        }
        case SymbolicAddress::Kind::BLOCK: {
          auto &block = addr.AsBlock().B;
          llvm::errs() << "TODO: " << block->getName() << "\n";
          continue;
        }
        case SymbolicAddress::Kind::STACK: {
          stacks.insert(addr.AsStack().Frame);
          continue;
        }
      }
      llvm_unreachable("invalid address kind");
    }
  }
}

// -----------------------------------------------------------------------------
void HeapGraph::Extract(Object *g, std::set<Func *> &funcs, BitSet<Node> &nodes)
{
  auto &n = nodes_[GetNode(g)];
  for (auto *f : n.funcs_) {
    funcs.insert(f);
  }
  nodes.Union(n.nodes_);
}

// -----------------------------------------------------------------------------
SymbolicValue HeapGraph::Build(
    const std::set<Func *> &funcs,
    const std::set<unsigned > &stacks,
    const BitSet<Node> &nodes)
{
  SymbolicPointer ptr;
  for (Func *f : funcs) {
    ptr.Add(f);
  }
  for (auto frame : stacks) {
    ptr.Add(frame);
  }
  for (auto [obj, id] : objectToNode_) {
    for (Atom &atom : *obj) {
      ptr.Add(&atom);
    }
  }
  for (auto [frame, id] : frameToNode_) {
    ptr.Add(frame.first, frame.second);
  }
  for (auto [alloc, id] : allocToNode_) {
    ptr.Add(alloc.first, alloc.second);
  }
  return SymbolicValue::Value(ptr);
}

// -----------------------------------------------------------------------------
ID<HeapGraph::Node>
HeapGraph::GetNode(Object *id)
{
  auto it = objectToNode_.emplace(id, nodes_.size());
  if (it.second) {
    nodes_.emplace_back(*this);
    nodes_[0].nodes_.Insert(it.first->second);
  }
  return it.first->second;
}

// -----------------------------------------------------------------------------
ID<HeapGraph::Node>
HeapGraph::GetNode(const std::pair<unsigned, unsigned> &id)
{
  auto it = frameToNode_.emplace(id, nodes_.size());
  if (it.second) {
    nodes_.emplace_back(*this);
    nodes_[0].nodes_.Insert(it.first->second);
  }
  return it.first->second;
}

// -----------------------------------------------------------------------------
ID<HeapGraph::Node>
HeapGraph::GetNode(const std::pair<unsigned, CallSite *> &id)
{
  auto it = allocToNode_.emplace(id, nodes_.size());
  if (it.second) {
    nodes_.emplace_back(*this);
    nodes_[0].nodes_.Insert(it.first->second);
  }
  return it.first->second;
}

// -----------------------------------------------------------------------------
void HeapGraph::Build(ID<Node> id, SymbolicObject &object)
{
  for (const auto &value : object) {
    if (auto ptr = value.AsPointer()) {
      for (auto &addr : *ptr) {
        switch (addr.GetKind()) {
          case SymbolicAddress::Kind::ATOM: {
            auto objectID = GetNode(addr.AsAtom().Symbol->getParent());
            nodes_[id].nodes_.Insert(objectID);
            continue;
          }
          case SymbolicAddress::Kind::ATOM_RANGE: {
            auto objectID = GetNode(addr.AsAtomRange().Symbol->getParent());
            nodes_[id].nodes_.Insert(objectID);
            continue;
          }
          case SymbolicAddress::Kind::FRAME: {
            auto &frame = addr.AsFrame();
            auto objectID = GetNode({ frame.Frame, frame.Object });
            nodes_[id].nodes_.Insert(objectID);
            continue;
          }
          case SymbolicAddress::Kind::FRAME_RANGE: {
            auto &frame = addr.AsFrameRange();
            auto objectID = GetNode({ frame.Frame, frame.Object });
            nodes_[id].nodes_.Insert(objectID);
            continue;
          }
          case SymbolicAddress::Kind::HEAP: {
            auto &alloc = addr.AsHeap();
            auto objectID = GetNode({ alloc.Frame, alloc.Alloc });
            nodes_[id].nodes_.Insert(objectID);
            continue;
          }
          case SymbolicAddress::Kind::HEAP_RANGE: {
            auto &alloc = addr.AsHeapRange();
            auto objectID = GetNode({ alloc.Frame, alloc.Alloc });
            nodes_[id].nodes_.Insert(objectID);
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
            nodes_[id].funcs_.insert(addr.AsFunc().Fn);
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
