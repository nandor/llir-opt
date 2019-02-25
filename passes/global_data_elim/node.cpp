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
  if (!deref_) {
    deref_ = new DerefNode(this);
  }
  return deref_;
}

// -----------------------------------------------------------------------------
void Node::AddEdge(Node *node)
{
  outs_.insert(node);
  node->ins_.insert(this);
}

// -----------------------------------------------------------------------------
RootNode::RootNode()
  : Node(Kind::ROOT)
  , node_(new SetNode())
{
}

// -----------------------------------------------------------------------------
RootNode::RootNode(uint64_t item)
  : Node(Kind::ROOT)
  , node_(new SetNode(item))
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
}
