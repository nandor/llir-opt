// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include "passes/pta/graph.h"
#include "passes/pta/node.h"



// -----------------------------------------------------------------------------
SetNode *Graph::Set()
{
  auto node = std::make_unique<SetNode>(sets_.size());
  auto *ptr = node.get();
  nodes_.emplace_back(std::move(node));
  sets_.push_back(ptr);
  unions_.emplace_back(ptr->GetID(), 0);
  return ptr;
}

// -----------------------------------------------------------------------------
DerefNode *Graph::Deref(SetNode *set)
{
  auto *contents = Root(Set());
  auto node = std::make_unique<DerefNode>(set, contents, derefs_.size());
  auto *ptr = node.get();
  ptr->AddSet(contents->Set());
  nodes_.emplace_back(std::move(node));
  derefs_.push_back(ptr);
  return ptr;
}

// -----------------------------------------------------------------------------
RootNode *Graph::Root(SetNode *set)
{
  auto node = std::make_unique<RootNode>(this, set);
  auto *ptr = node.get();
  nodes_.push_back(std::move(node));
  roots_.push_back(ptr);
  return ptr;
}

// -----------------------------------------------------------------------------
SetNode *Graph::Find(ID<SetNode *> id)
{
  uint32_t root = id;
  while (unions_[root].Parent != root) {
    root = unions_[root].Parent;
  }
  while (unions_[id].Parent != id) {
    uint32_t parent = unions_[id].Parent;
    unions_[id].Parent = root;
    id = parent;
  }
  return sets_[id];
}

// -----------------------------------------------------------------------------
SetNode *Graph::Union(SetNode *a, SetNode *b)
{
  if (!a || a == b) {
    return b;
  }

  const uint32_t idA = a->GetID(), idB = b->GetID();
  Entry &entryA = unions_[idA], &entryB = unions_[idB];

  SetNode *node;
  if (entryA.Rank < entryB.Rank) {
    entryA.Parent = idB;
    a->Propagate(b);
    Replace(a, b);
    sets_[idA] = nullptr;
    node = b;
  } else {
    entryB.Parent = idA;
    b->Propagate(a);
    Replace(b, a);
    sets_[idB] = nullptr;
    node = a;
  }

  if (entryA.Rank == entryB.Rank) {
    entryA.Rank += 1;
  }
  return node;
}

// -----------------------------------------------------------------------------
void Graph::Replace(SetNode *a, SetNode *b)
{
  assert(a != b && "Attempting to replace pointer with self");

  b->sets_.Union(a->sets_);
  b->derefIns_.Union(a->derefIns_);
  b->derefOuts_.Union(a->derefOuts_);

  if (a->deref_) {
    if (b->deref_) {
      Replace(a->deref_, b->deref_);
    } else {
      b->deref_ = a->deref_;
      b->deref_->node_ = b;
    }
    a->deref_ = nullptr;
  }
}

// -----------------------------------------------------------------------------
void Graph::Replace(DerefNode *a, DerefNode *b)
{
  for (auto inID : a->setIns_) {
    auto *in = Find(inID);
    in->derefOuts_.Erase(a->id_);
    in->derefOuts_.Insert(b->id_);
    b->setIns_.Insert(inID);
  }

  for (auto outID : a->setOuts_) {
    auto *out = Find(outID);
    out->derefIns_.Erase(a->id_);
    out->derefIns_.Insert(b->id_);
    b->setOuts_.Insert(outID);
  }
}
