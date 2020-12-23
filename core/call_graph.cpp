// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include "core/call_graph.h"
#include "core/prog.h"
#include "core/block.h"
#include "core/insts.h"



// -----------------------------------------------------------------------------
static Inst *Next(Inst *inst)
{
  Block *block = inst->getParent();
  Func *func = block->getParent();

  auto it = std::next(inst->getIterator());
  if (it != block->end()) {
    return &*it;
  }

  auto bt = std::next(block->getIterator());
  if (bt != func->end()) {
    return &*bt->begin();
  }

  return nullptr;
}

// -----------------------------------------------------------------------------
CallGraph::Node::iterator::iterator(const Node *node, Inst *start)
  : node_(node)
{
  while (start && !GetCallee(start)) {
    start = Next(start);
  }
  it_ = start;
}

// -----------------------------------------------------------------------------
CallGraph::Node::iterator::iterator(const Node *node, Func *func)
  : node_(node), it_(func)
{
}

// -----------------------------------------------------------------------------
bool CallGraph::Node::iterator::operator==(const iterator &that) const
{
  return (it_.isNull() && that.it_.isNull()) || it_ == that.it_;
}

// -----------------------------------------------------------------------------
CallGraph::Node::iterator &CallGraph::Node::iterator::operator++()
{
  if (Inst *inst = it_.dyn_cast<Inst *>()) {
    while ((inst = Next(inst)) && !GetCallee(inst));
    it_ = inst;
    return *this;
  }
  if (Func *func = it_.dyn_cast<Func *>()) {
    auto it = ++func->getIterator();
    if (it != func->getParent()->end()) {
      it_ = &*it;
    } else {
      it_ = static_cast<Func *>(nullptr);
    }
    return *this;
  }
  llvm_unreachable("invalid iterator");
}

// -----------------------------------------------------------------------------
const CallGraph::Node *CallGraph::Node::iterator::operator*() const
{
  if (Inst *inst = it_.dyn_cast<Inst *>()) {
    return (*node_->graph_)[GetCallee(inst)];
  }
  if (Func *func = it_.dyn_cast<Func *>()) {
    return (*node_->graph_)[func];
  }
  llvm_unreachable("invalid iterator");
}

// -----------------------------------------------------------------------------
CallGraph::Node::Node(const CallGraph *graph, Prog *prog)
  : graph_(graph), node_(prog)
{
}

// -----------------------------------------------------------------------------
CallGraph::Node::Node(const CallGraph *graph, Func *caller)
  : graph_(graph), node_(caller)
{
}

// -----------------------------------------------------------------------------
CallGraph::Node::iterator CallGraph::Node::begin() const
{
  if (Func *f = node_.dyn_cast<Func *>()) {
    return iterator(this, &*f->getEntryBlock().begin());
  }
  if (Prog *p = node_.dyn_cast<Prog *>()) {
    if (p->empty()) {
      return iterator();
    } else {
      return iterator(this, &*p->begin());
    }
  }
  llvm_unreachable("invalid node");
}

// -----------------------------------------------------------------------------
Func *CallGraph::Node::GetCaller() const
{
  return node_.dyn_cast<Func *>();
}

// -----------------------------------------------------------------------------
bool CallGraph::Node::IsRecursive() const
{
  if (auto *f = GetCaller()) {
    for (auto *node : *this) {
      if (f == node->GetCaller()) {
        return true;
      }
    }
  }
  return false;
}

// -----------------------------------------------------------------------------
CallGraph::CallGraph(Prog &p)
  : entry_(this, &p)
{
}

// -----------------------------------------------------------------------------
CallGraph::~CallGraph()
{
}

// -----------------------------------------------------------------------------
const CallGraph::Node *CallGraph::operator[](Func *f) const
{
  assert(f && "invalid function");
  auto it = nodes_.emplace(f, nullptr);
  if (it.second) {
    it.first->second = std::make_unique<Node>(this, f);
  }
  return it.first->second.get();
}