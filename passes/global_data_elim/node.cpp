// This file if part of the genm-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include "passes/global_data_elim/node.h"


// -----------------------------------------------------------------------------
Node::Node()
  : deref_(nullptr)
{
}

// -----------------------------------------------------------------------------
Node::Node(uint64_t item)
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
  outs_.push_back(node);
  node->ins_.push_back(this);
}

// -----------------------------------------------------------------------------
RootNode::RootNode()
  : node_(new Node())
{
}

// -----------------------------------------------------------------------------
RootNode::RootNode(uint64_t item)
  : node_(new Node(item))
{
}

// -----------------------------------------------------------------------------
DerefNode::DerefNode(Node *node)
  : node_(node)
{
}
