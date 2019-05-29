// This file if part of the genm-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include "passes/pta/node.h"
#include "passes/pta/graph.h"



// -----------------------------------------------------------------------------
Node::Node(Kind kind)
  : kind_(kind)
{
}

// -----------------------------------------------------------------------------
Node::~Node()
{
}

// -----------------------------------------------------------------------------
GraphNode *Node::ToGraph()
{
  switch (kind_) {
    case Kind::SET: return static_cast<SetNode *>(this);
    case Kind::DEREF: return static_cast<DerefNode *>(this);
    case Kind::ROOT: return static_cast<RootNode *>(this)->Set();
  }
}

// -----------------------------------------------------------------------------
RootNode *Node::AsRoot()
{
  return kind_ == Kind::ROOT ? static_cast<RootNode *>(this) : nullptr;
}

// -----------------------------------------------------------------------------
GraphNode::GraphNode(Kind kind, uint64_t id)
  : Node(kind)
  , id_(id)
  , Epoch(0ull)
  , Index(0)
  , Link(0)
  , InComponent(false)
{
}

// -----------------------------------------------------------------------------
GraphNode::~GraphNode()
{
}

// -----------------------------------------------------------------------------
SetNode *GraphNode::AsSet()
{
  return IsSet() ? static_cast<SetNode *>(this) : nullptr;
}

// -----------------------------------------------------------------------------
DerefNode *GraphNode::AsDeref()
{
  return IsDeref() ? static_cast<DerefNode *>(this) : nullptr;
}

// -----------------------------------------------------------------------------
SetNode::SetNode(uint64_t id)
  : GraphNode(Kind::SET, id)
  , deref_(nullptr)
{
}

// -----------------------------------------------------------------------------
SetNode::~SetNode()
{
}

// -----------------------------------------------------------------------------
DerefNode *SetNode::Deref()
{
  return deref_;
}

// -----------------------------------------------------------------------------
bool SetNode::Propagate(SetNode *that)
{
  bool changed = false;
  changed |= that->funcs_.Union(funcs_);
  changed |= that->exts_.Union(exts_);
  changed |= that->nodes_.Union(nodes_);
  return changed;
}

// -----------------------------------------------------------------------------
bool SetNode::AddSet(SetNode *node)
{
  return sets_.Insert(node->id_);
}

// -----------------------------------------------------------------------------
bool SetNode::AddDeref(DerefNode *node)
{
  if (derefOuts_.Insert(node->id_)) {
    node->setIns_.Insert(id_);
    return true;
  } else {
    return false;
  }
}

// -----------------------------------------------------------------------------
bool SetNode::Equals(SetNode *that)
{
  return funcs_ == that->funcs_ && exts_ == that->exts_ && nodes_ == that->nodes_;
}

// -----------------------------------------------------------------------------
void SetNode::sets(std::function<ID<SetNode *>(ID<SetNode *>)> &&f)
{
  std::vector<std::pair<uint32_t, uint32_t>> fixups;

  for (auto id : sets()) {
    auto newID = f(id);
    if (newID != id) {
      fixups.emplace_back(id, newID);
    }
  }

  for (const auto &fixup : fixups) {
    sets_.Erase(fixup.first);
    sets_.Insert(fixup.second);
  }
}

// -----------------------------------------------------------------------------
void SetNode::points_to_node(std::function<ID<SetNode *>(ID<SetNode *>)> &&f)
{
  std::vector<std::pair<uint32_t, uint32_t>> fixups;

  for (auto id : points_to_node()) {
    auto newID = f(id);
    if (newID != id) {
      fixups.emplace_back(id, newID);
    }
  }

  for (const auto &fixup : fixups) {
    nodes_.Erase(fixup.first);
    nodes_.Insert(fixup.second);
  }
}

// -----------------------------------------------------------------------------
DerefNode::DerefNode(SetNode *node, RootNode *contents, uint64_t id)
  : GraphNode(Kind::DEREF, id)
  , node_(node)
  , contents_(contents)
{
  node_->deref_ = this;
}

// -----------------------------------------------------------------------------
DerefNode::~DerefNode()
{
}

// -----------------------------------------------------------------------------
SetNode *DerefNode::Contents()
{
  return contents_->Set();
}

// -----------------------------------------------------------------------------
bool DerefNode::AddSet(SetNode *node)
{
  if (setOuts_.Insert(node->id_)) {
    node->derefIns_.Insert(id_);
    return true;
  } else {
    return false;
  }
}

// -----------------------------------------------------------------------------
void DerefNode::set_ins(std::function<ID<SetNode *>(ID<SetNode *>)> &&f)
{
  std::vector<std::pair<uint32_t, uint32_t>> fixups;

  for (auto id : set_ins()) {
    auto newID = f(id);
    if (newID != id) {
      fixups.emplace_back(id, newID);
    }
  }

  for (const auto &fixup : fixups) {
    setIns_.Erase(fixup.first);
    setIns_.Insert(fixup.second);
  }
}

// -----------------------------------------------------------------------------
void DerefNode::set_outs(std::function<ID<SetNode *>(ID<SetNode *>)> &&f)
{
  std::vector<std::pair<uint32_t, uint32_t>> fixups;

  for (auto id : set_outs()) {
    auto newID = f(id);
    if (newID != id) {
      fixups.emplace_back(id, newID);
    }
  }

  for (const auto &fixup : fixups) {
    setOuts_.Erase(fixup.first);
    setOuts_.Insert(fixup.second);
  }
}

// -----------------------------------------------------------------------------
RootNode::RootNode(Graph *graph, SetNode *actual)
  : Node(Kind::ROOT)
  , graph_(graph)
  , id_(actual->GetID())
{
}

// -----------------------------------------------------------------------------
SetNode *RootNode::Set() const
{
  SetNode *set = graph_->Find(id_);
  id_ = set->GetID();
  return set;
}
