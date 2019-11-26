// This file if part of the genm-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include "passes/local_const/context.h"
#include "passes/local_const/graph.h"



// -----------------------------------------------------------------------------
LCContext::LCContext(LCGraph &graph)
  : graph_(graph)
  , extern_(graph_.Set()->GetID())
  , root_(graph_.Set()->GetID())
{
}

// -----------------------------------------------------------------------------
LCGraph &LCContext::Graph()
{
  return graph_;
}

// -----------------------------------------------------------------------------
LCSet *LCContext::Extern()
{
  return graph_.Find(extern_);
}

// -----------------------------------------------------------------------------
LCSet *LCContext::Root()
{
  return graph_.Find(root_);
}

// -----------------------------------------------------------------------------
LCSet *LCContext::MapNode(const Inst *inst, LCSet *node)
{
  ID<LCSet> id = node->GetID();
  nodes_.emplace(inst, id).first->second = id;
  return node;
}

// -----------------------------------------------------------------------------
LCSet *LCContext::GetNode(const Inst *inst)
{
  if (auto it = nodes_.find(inst); it != nodes_.end()) {
    return graph_.Find(it->second);
  }
  return nullptr;
}

// -----------------------------------------------------------------------------
LCSet *LCContext::MapLive(const Inst *inst, LCSet *node)
{
  ID<LCSet> id = node->GetID();
  lives_.emplace(inst, id).first->second = id;
  return node;
}

// -----------------------------------------------------------------------------
LCSet *LCContext::GetLive(const Inst *inst)
{
  if (auto it = lives_.find(inst); it != lives_.end()) {
    return graph_.Find(it->second);
  }
  return nullptr;
}
