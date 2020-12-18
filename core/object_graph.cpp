// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include "core/block.h"
#include "core/insts.h"
#include "core/object_graph.h"
#include "core/prog.h"



// -----------------------------------------------------------------------------
static Item *Next(Item *item)
{
  Atom *atom = item->getParent();
  Object *object = atom->getParent();

  auto it = std::next(item->getIterator());
  if (it != atom->end()) {
    return &*it;
  }

  auto at = atom->getIterator();
  while (++at != object->end()) {
    if (!at->empty()) {
      return &*at->begin();
    }
  }

  return nullptr;
}

// -----------------------------------------------------------------------------
static Global *ToGlobal(Item *item)
{
  if (auto *expr = item->AsExpr()) {
    switch (expr->GetKind()) {
      case Expr::Kind::SYMBOL_OFFSET: {
        return static_cast<SymbolOffsetExpr *>(expr)->GetSymbol();
      }
    }
    llvm_unreachable("invalid expression kind");
  }
  return nullptr;
}

// -----------------------------------------------------------------------------
static Object *ToObject(Item *item)
{
  if (auto *g = ToGlobal(item)) {
    switch (g->GetKind()) {
      case Global::Kind::EXTERN:
      case Global::Kind::FUNC:
      case Global::Kind::BLOCK: {
        return nullptr;
      }
      case Global::Kind::ATOM: {
        return static_cast<Atom *>(g)->getParent();
      }
    }
    llvm_unreachable("invalid global kind");
  }
  return nullptr;
}

// -----------------------------------------------------------------------------
ObjectGraph::Node::iterator::iterator(const Node *node, Item *start)
  : node_(node)
{
  while (start && !ToObject(start)) {
    start = Next(start);
  }
  it_ = start;
}

// -----------------------------------------------------------------------------
ObjectGraph::Node::iterator::iterator(const Node *node, Object *object)
  : node_(node), it_(object)
{
}

// -----------------------------------------------------------------------------
bool ObjectGraph::Node::iterator::operator==(const iterator &that) const
{
  return (it_.isNull() && that.it_.isNull()) || it_ == that.it_;
}

// -----------------------------------------------------------------------------
ObjectGraph::Node::iterator &ObjectGraph::Node::iterator::operator++()
{
  if (Item *inst = it_.dyn_cast<Item *>()) {
    while ((inst = Next(inst)) && !ToObject(inst));
    it_ = inst;
    return *this;
  }
  if (Object *obj = it_.dyn_cast<Object *>()) {
    auto it = ++obj->getIterator();
    if (it != obj->getParent()->end()) {
      it_ = &*it;
    } else {
      auto dt = std::next(obj->getParent()->getIterator());
      if (dt != obj->getParent()->getParent()->data_end()) {
        it_ = &*dt->begin();
      } else {
        it_ = static_cast<Object *>(nullptr);
      }
    }
    return *this;
  }
  llvm_unreachable("invalid iterator");
}

// -----------------------------------------------------------------------------
ObjectGraph::Node *ObjectGraph::Node::iterator::operator*() const
{
  if (Item *inst = it_.dyn_cast<Item *>()) {
    return (*node_->graph_)[ToObject(inst)];
  }
  if (Object *object = it_.dyn_cast<Object *>()) {
    return (*node_->graph_)[object];
  }
  llvm_unreachable("invalid iterator");
}

// -----------------------------------------------------------------------------
ObjectGraph::Node::Node(const ObjectGraph *graph, Prog *prog)
  : graph_(graph), node_(prog)
{
}

// -----------------------------------------------------------------------------
ObjectGraph::Node::Node(const ObjectGraph *graph, Object *object)
  : graph_(graph), node_(object)
{
}

// -----------------------------------------------------------------------------
ObjectGraph::Node::iterator ObjectGraph::Node::begin() const
{
  if (Object *obj = node_.dyn_cast<Object *>()) {
    if (obj->empty() || obj->begin()->empty()) {
      return iterator();
    }
    return iterator(this, &*obj->begin()->begin());
  }
  if (Prog *p = node_.dyn_cast<Prog *>()) {
    if (p->empty()) {
      return iterator();
    } else {
      return iterator(this, &*p->data_begin()->begin());
    }
  }
  llvm_unreachable("invalid node");
}

// -----------------------------------------------------------------------------
Object *ObjectGraph::Node::GetObject() const
{
  return node_.dyn_cast<Object *>();
}

// -----------------------------------------------------------------------------
ObjectGraph::ObjectGraph(Prog &p)
  : entry_(this, &p)
{
}

// -----------------------------------------------------------------------------
ObjectGraph::~ObjectGraph()
{
}

// -----------------------------------------------------------------------------
ObjectGraph::Node *ObjectGraph::operator[](Object *o) const
{
  assert(o && "invalid object");
  auto it = nodes_.emplace(o, nullptr);
  if (it.second) {
    it.first->second = std::make_unique<Node>(this, o);
  }
  return it.first->second.get();
}
