// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include "core/block.h"
#include "core/insts.h"
#include "core/object_graph.h"
#include "core/prog.h"



// -----------------------------------------------------------------------------
static const Item *Next(const Item *item)
{
  const Atom *atom = item->getParent();
  const Object *object = atom->getParent();

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
static Global *ToGlobal(const Item *item)
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
static Object *ToObject(const Item *item)
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
ObjectGraph::Node::iterator::iterator(const Node *node, const Item *start)
  : node_(node)
{
  while (start && !ToObject(start)) {
    start = Next(start);
  }
  it_ = start;
}

// -----------------------------------------------------------------------------
ObjectGraph::Node::iterator::iterator(const Node *node, const Object *object)
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
  if (const Item *inst = it_.dyn_cast<const Item *>()) {
    while ((inst = Next(inst)) && !ToObject(inst));
    it_ = inst;
    return *this;
  }
  if (const Object *obj = it_.dyn_cast<const Object *>()) {
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
  if (const Item *inst = it_.dyn_cast<const Item *>()) {
    return (*node_->graph_)[ToObject(inst)];
  }
  if (const Object *object = it_.dyn_cast<const Object *>()) {
    return (*node_->graph_)[object];
  }
  llvm_unreachable("invalid iterator");
}

// -----------------------------------------------------------------------------
ObjectGraph::Node::Node(const ObjectGraph *graph, const Prog *prog)
  : graph_(graph), node_(prog)
{
}

// -----------------------------------------------------------------------------
ObjectGraph::Node::Node(const ObjectGraph *graph, const Object *object)
  : graph_(graph), node_(object)
{
}

// -----------------------------------------------------------------------------
ObjectGraph::Node::iterator ObjectGraph::Node::begin() const
{
  if (const Object *obj = node_.dyn_cast<const Object *>()) {
    if (obj->empty() || obj->begin()->empty()) {
      return iterator();
    }
    return iterator(this, &*obj->begin()->begin());
  }
  if (const Prog *p = node_.dyn_cast<const Prog *>()) {
    if (p->empty()) {
      return iterator();
    } else {
      return iterator(this, &*p->data_begin()->begin());
    }
  }
  llvm_unreachable("invalid node");
}

// -----------------------------------------------------------------------------
const Object *ObjectGraph::Node::GetObject() const
{
  return node_.dyn_cast<const Object *>();
}

// -----------------------------------------------------------------------------
ObjectGraph::ObjectGraph(const Prog &p)
  : entry_(this, &p)
{
}

// -----------------------------------------------------------------------------
ObjectGraph::~ObjectGraph()
{
}

// -----------------------------------------------------------------------------
ObjectGraph::Node *ObjectGraph::operator[](const Object *o) const
{
  assert(o && "invalid object");
  auto it = nodes_.emplace(o, nullptr);
  if (it.second) {
    it.first->second = std::make_unique<Node>(this, o);
  }
  return it.first->second.get();
}
