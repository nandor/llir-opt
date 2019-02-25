// This file if part of the genm-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include "passes/global_data_elim/node.h"



// -----------------------------------------------------------------------------
Node::Node(Kind kind)
  : kind_(kind)
  , deref_(nullptr)
{
}

// -----------------------------------------------------------------------------
Node *Node::Deref()
{
  return deref_;
}

// -----------------------------------------------------------------------------
void Node::AddEdge(Node *node)
{
  outs_.insert(node);
  node->ins_.insert(this);
}

// -----------------------------------------------------------------------------
RootNode::RootNode(SetNode *actual)
  : Node(Kind::ROOT)
  , actual_(actual)
{
}

// -----------------------------------------------------------------------------
SetNode::SetNode()
  : Node(Kind::SET)
{
}

// -----------------------------------------------------------------------------
SetNode::SetNode(uint64_t item)
  : Node(Kind::SET)
{
}

// -----------------------------------------------------------------------------
DerefNode::DerefNode(Node *node)
  : Node(Kind::DEREF)
  , node_(node)
{
  if (node_->kind_ == Kind::ROOT) {
    static_cast<RootNode *>(node_)->actual_->deref_ = this;
  } else {
    node_->deref_ = this;
  }
}
