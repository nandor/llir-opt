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
GraphNode::GraphNode(Kind kind)
  : Node(kind)
  , deref_(nullptr)
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
void GraphNode::AddEdge(GraphNode *node)
{
  outs_.insert(node);
  node->ins_.insert(this);
}

// -----------------------------------------------------------------------------
void GraphNode::RemoveEdge(GraphNode *node)
{
  outs_.erase(node);
  node->ins_.erase(this);
}

// -----------------------------------------------------------------------------
SetNode *GraphNode::AsSet()
{
  return IsSet() ? static_cast<SetNode *>(this) : nullptr;
}

// -----------------------------------------------------------------------------
SetNode::SetNode()
  : GraphNode(Kind::SET)
{
}

// -----------------------------------------------------------------------------
SetNode::SetNode(uint64_t item)
  : GraphNode(Kind::SET)
{
}

// -----------------------------------------------------------------------------
bool SetNode::Propagate(SetNode *that)
{
  bool changed = false;
  for (auto item : items_) {
    changed |= that->items_.insert(item).second;
  }
  return changed;
}

// -----------------------------------------------------------------------------
void SetNode::Replace(SetNode *that)
{
  for (auto it = roots_.begin(); it != roots_.end(); ) {
    RootNode *root = &*it++;

    root->actual_ = that;
    roots_.erase(root->getIterator());
    that->roots_.push_back(root);
  }

  for (auto *in : ins_) {
    in->outs_.erase(this);
    in->outs_.insert(that);
  }

  for (auto *out : outs_) {
    out->ins_.erase(this);
    out->ins_.insert(that);
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
DerefNode::DerefNode(GraphNode *node)
  : GraphNode(Kind::DEREF)
  , node_(node)
{
  node_->deref_ = this;
}

// -----------------------------------------------------------------------------
void DerefNode::Replace(DerefNode *that)
{
  for (auto *in : ins_) {
    in->outs_.erase(this);
    in->outs_.insert(that);
  }

  for (auto *out : outs_) {
    out->ins_.erase(this);
    out->ins_.insert(that);
  }

  if (deref_) {
    if (that->deref_) {
      deref_->Replace(that->deref_);
    } else {
      that->deref_ = deref_;
      deref_->node_ = that;
    }
  }
}

// -----------------------------------------------------------------------------
RootNode::RootNode(SetNode *actual)
  : Node(Kind::ROOT)
  , actual_(actual)
{
  actual_->roots_.push_back(this);
}
