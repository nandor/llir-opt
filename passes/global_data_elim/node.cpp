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
  , Index(0)
  , Link(0)
  , OnStack(false)
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
bool SetNode::AddEdge(SetNode *node)
{
  if (setOuts_.insert(node).second) {
    node->setIns_.insert(this);
    return true;
  } else {
    return false;
  }
}

// -----------------------------------------------------------------------------
void SetNode::RemoveEdge(SetNode *node)
{
  setOuts_.erase(node);
  node->setIns_.erase(this);
}

// -----------------------------------------------------------------------------
bool SetNode::AddEdge(DerefNode *node)
{
  if (derefOuts_.insert(node).second) {
    node->setIns_.insert(this);
    return true;
  } else {
    return false;
  }
}

// -----------------------------------------------------------------------------
void SetNode::RemoveEdge(DerefNode *node)
{
  derefOuts_.erase(node);
  node->setIns_.erase(this);
}

// -----------------------------------------------------------------------------
void SetNode::Replace(SetNode *that)
{
  for (auto *root : roots_) {
    root->actual_ = that;
    that->roots_.insert(root);
  }

  for (auto *in : setIns_) {
    in->setOuts_.erase(this);
    in->setOuts_.insert(that);
    that->setIns_.insert(in);
  }

  for (auto *out : setOuts_) {
    out->setIns_.erase(this);
    out->setIns_.insert(that);
    that->setOuts_.insert(out);
  }

  for (auto *in : derefIns_) {
    in->setOuts_.erase(this);
    in->setOuts_.insert(that);
    that->derefIns_.insert(in);
  }

  for (auto *out : derefOuts_) {
    out->setIns_.erase(this);
    out->setIns_.insert(that);
    that->derefOuts_.insert(out);
  }

  if (deref_) {
    if (that->deref_) {
      deref_->Replace(that->deref_);
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
DerefNode::DerefNode(GraphNode *node, uint64_t id)
  : GraphNode(Kind::DEREF, id)
  , node_(node)
{
  node_->deref_ = this;
}

// -----------------------------------------------------------------------------
bool DerefNode::AddEdge(SetNode *node)
{
  if (setOuts_.insert(node).second) {
    node->derefIns_.insert(this);
    return true;
  } else {
    return false;
  }
}

// -----------------------------------------------------------------------------
void DerefNode::RemoveEdge(SetNode *node)
{
  setOuts_.erase(node);
  node->derefIns_.erase(this);
}

// -----------------------------------------------------------------------------
void DerefNode::Replace(DerefNode *that)
{
  for (auto *in : setIns_) {
    in->derefOuts_.erase(this);
    in->derefOuts_.insert(that);
  }

  for (auto *out : setOuts_) {
    out->derefIns_.erase(this);
    out->derefIns_.insert(that);
  }

  if (deref_) {
    if (that->deref_) {
      deref_->Replace(that->deref_);
    } else {
      that->deref_ = deref_;
      deref_->node_ = that;
    }
    deref_ = nullptr;
  }
}

// -----------------------------------------------------------------------------
RootNode::RootNode(BitSet<RootNode *>::Item id, SetNode *actual)
  : Node(Kind::ROOT)
  , id_(id)
  , actual_(actual)
{
  actual_->roots_.insert(this);
}
