// This file if part of the genm-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include "passes/global_data_elim/node.h"



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
    case Kind::ROOT: return static_cast<RootNode *>(this)->actual_;
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
  , deref_(nullptr)
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
DerefNode *GraphNode::Deref()
{
  return deref_;
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
{
}

// -----------------------------------------------------------------------------
SetNode::~SetNode()
{
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
  if (setOuts_.Insert(node->id_)) {
    node->setIns_.Insert(id_);
    return true;
  } else {
    return false;
  }
}

// -----------------------------------------------------------------------------
void SetNode::RemoveSet(SetNode *node)
{
  setOuts_.Erase(node->id_);
  node->setIns_.Erase(id_);
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
void SetNode::RemoveDeref(DerefNode *node)
{
  derefOuts_.Erase(node->id_);
  node->setIns_.Erase(id_);
}

// -----------------------------------------------------------------------------
void SetNode::Replace(
      const std::vector<std::unique_ptr<SetNode>> &sets,
      const std::vector<std::unique_ptr<DerefNode>> &derefs,
      SetNode *that)
{
  assert(this != that && "Attempting to replace pointer with self");

  for (auto *root : roots_) {
    root->actual_ = that;
    that->roots_.insert(root);
  }
  roots_.clear();

  for (auto inID : setIns_) {
    auto *in = sets.at(inID).get();
    in->setOuts_.Erase(id_);
    in->setOuts_.Insert(that->id_);
    that->setIns_.Insert(in->id_);
  }
  setIns_.Clear();

  for (auto outID : setOuts_) {
    auto *out = sets.at(outID).get();
    out->setIns_.Erase(id_);
    out->setIns_.Insert(that->id_);
    that->setOuts_.Insert(out->id_);
  }
  setOuts_.Clear();

  for (auto inID : derefIns_) {
    auto *in = derefs.at(inID).get();
    in->setOuts_.Erase(id_);
    in->setOuts_.Insert(that->id_);
    that->derefIns_.Insert(in->id_);
  }
  derefIns_.Clear();

  for (auto outID : derefOuts_) {
    auto *out = derefs.at(outID).get();
    out->setIns_.Erase(id_);
    out->setIns_.Insert(that->id_);
    that->derefOuts_.Insert(out->id_);
  }
  derefOuts_.Clear();

  if (deref_) {
    if (that->deref_) {
      deref_->Replace(sets, that->deref_);
    } else {
      that->deref_ = deref_;
      deref_->node_ = that;
    }
    deref_ = nullptr;
  }
}

// -----------------------------------------------------------------------------
bool SetNode::Equals(SetNode *that)
{
  return funcs_ == that->funcs_ && exts_ == that->exts_ && nodes_ == that->nodes_;
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
void DerefNode::RemoveSet(SetNode *node)
{
  setOuts_.Erase(node->id_);
  node->derefIns_.Erase(id_);
}

// -----------------------------------------------------------------------------
void DerefNode::Replace(
    const std::vector<std::unique_ptr<SetNode>> &sets,
    DerefNode *that)
{
  for (auto inID : setIns_) {
    auto *in = sets.at(inID).get();
    in->derefOuts_.Erase(id_);
    in->derefOuts_.Insert(that->id_);
  }
  setIns_.Clear();

  for (auto outID : setOuts_) {
    auto *out = sets.at(outID).get();
    out->derefIns_.Erase(id_);
    out->derefIns_.Insert(that->id_);
  }
  setOuts_.Clear();

  if (deref_) {
    if (that->deref_) {
      deref_->Replace(sets, that->deref_);
    } else {
      that->deref_ = deref_;
      deref_->node_ = that->Contents();
    }
    deref_ = nullptr;
  }
}

// -----------------------------------------------------------------------------
RootNode::RootNode(SetNode *actual)
  : Node(Kind::ROOT)
  , actual_(actual)
{
  actual_->roots_.insert(this);
}

// -----------------------------------------------------------------------------
HeapNode::HeapNode(BitSet<RootNode *>::Item id, SetNode *actual)
  : RootNode(actual)
  , id_(id)
{
}
