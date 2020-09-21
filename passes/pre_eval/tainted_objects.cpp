// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include <stack>

#include <llvm/ADT/PostOrderIterator.h>
#include <llvm/ADT/SCCIterator.h>

#include "core/atom.h"
#include "core/block.h"
#include "core/call_graph.h"
#include "core/cast.h"
#include "core/cfg.h"
#include "core/extern.h"
#include "core/func.h"
#include "core/inst_visitor.h"
#include "core/insts.h"
#include "core/object.h"
#include "core/prog.h"
#include "passes/pre_eval/single_execution.h"
#include "passes/pre_eval/tainted_objects.h"



// -----------------------------------------------------------------------------
using BlockInfo = TaintedObjects::BlockInfo;


// -----------------------------------------------------------------------------
bool TaintedObjects::Tainted::Union(const Tainted &that)
{
  bool changed = false;
  changed |= objects_.Union(that.objects_);
  changed |= funcs_.Union(that.funcs_);
  changed |= blocks_.Union(that.blocks_);
  return changed;
}

// -----------------------------------------------------------------------------
TaintedObjects::TaintedObjects(Func &entry)
  : graph_(*entry.getParent())
  , single_(SingleExecution(entry).Solve())
  , entry_(Explore(CallString(nullptr), entry).Entry)
{
  /*
  for (auto single : single_) {
    llvm::errs() << single->getName() << "\n";
  }
  llvm::errs() << "\n\n";
  */
  do {
    Propagate();
  } while (ExpandIndirect());
}

// -----------------------------------------------------------------------------
TaintedObjects::~TaintedObjects()
{
}

// -----------------------------------------------------------------------------
std::optional<TaintedObjects::Tainted>
TaintedObjects::operator[](const Inst &inst) const
{
  if (auto it = blockSites_.find(&inst); it != blockSites_.end()) {
    Tainted tainted;
    for (auto blockID : it->second) {
      tainted.Union(blocks_.Map(blockID)->Taint);
    }
    return tainted;
  }
  return std::nullopt;
}

// -----------------------------------------------------------------------------
void TaintedObjects::ExploreQueue()
{
  while (!explore_.empty()) {
    auto item = explore_.front();
    explore_.pop();

    auto itemID = Visit(item.CS, *item.F);
    auto site = blocks_.Map(item.Site);
    site->Successors.Insert(itemID.Entry);
    for (auto exitID : itemID.Exit) {
      auto *exit = blocks_.Map(exitID);
      for (auto cont : item.Cont) {
        exit->Successors.Insert(cont);
      }
    }
  }
}

// -----------------------------------------------------------------------------
ID<BlockInfo> TaintedObjects::Visit(
    const CallString &cs,
    const FlowGraph::Node *node,
    BitSet<BlockInfo> *exits)
{
  // Advance the call string if the node is for an entry method.
  bool isEntry = false;
  for (auto instID : node->Origins) {
    const Block *block = graph_[instID]->getParent();
    if (single_.count(block)) {
      isEntry = true;
    }
  }
  CallString fcs = isEntry ? cs.Context(node) : cs;

  // De-duplicate nodes.
  Key<const FlowGraph::Node *> key{ fcs, node };
  {
    auto it = blockIDs_.find(key);
    if (it != blockIDs_.end()) {
      return it->second;
    }
  }

  auto blockID = blocks_.Emplace(this, node);
  blockIDs_.emplace(key, blockID);
  for (auto instID : node->Origins) {
    blockSites_[graph_[instID]].Insert(blockID);
  }

  BitSet<BlockInfo> successors;
  for (auto succID : node->Successors) {
    successors.Insert(Visit(fcs, graph_[succID], exits));
  }

  /*
  llvm::errs() << "NODE:\n";
  for (auto instID : node->Origins) {
    const Block *block = graph_[instID]->getParent();
    llvm::errs() << block->getName() << "\n";
  }
  llvm::errs() << "\n";
  */

  if (node->IsExit && exits) {
    exits->Insert(blockID);
  }

  if (node->Callee) {
    explore_.emplace(
        fcs,
        node->Callee,
        blockID,
        node->IsLoop ? BitSet<BlockInfo>{ blockID } : successors
    );
  }

  if (node->HasIndirectJumps) {
    indirectJumps_.emplace_back(fcs, blockID);
  }

  if (node->HasIndirectCalls) {
    indirectCalls_.emplace_back(
        fcs,
        blockID,
        node->IsLoop ? BitSet<BlockInfo>{ blockID } : successors
    );
  }

  blocks_.Map(blockID)->Successors = std::move(successors);

  return blockID;
}

// -----------------------------------------------------------------------------
TaintedObjects::FunctionID &TaintedObjects::Visit(
    const CallString &cs,
    const Func &func)
{
  BitSet<BlockInfo> exits;
  auto entryID = Visit(cs, graph_[graph_[&func]], &exits);
  /*
  if (func.getName() == "caml_start_program") {
    abort();
  }
  */
  auto it = funcIDs_.emplace(entryID, FunctionID{ entryID, std::move(exits) });
  return it.first->second;
}

// -----------------------------------------------------------------------------
template <>
struct llvm::GraphTraits<BlockInfo *> {
  using NodeRef = BlockInfo *;
  using ChildIteratorType = BlockInfo::iterator;

  static NodeRef getEntryNode(BlockInfo* BB) { return BB; }
  static ChildIteratorType child_begin(NodeRef N) { return N->begin(); }
  static ChildIteratorType child_end(NodeRef N) { return N->end(); }
};

// -----------------------------------------------------------------------------
template <>
struct llvm::GraphTraits<TaintedObjects *>
    : public llvm::GraphTraits<BlockInfo *>
{
  static NodeRef getEntryNode(TaintedObjects *G) { return G->GetEntryNode(); }
};


// -----------------------------------------------------------------------------
void TaintedObjects::Propagate()
{
  llvm::errs() << blocks_.Size() << "\n";
  // Find the SCCs in the graph of block nodes.
  std::vector<ID<BlockInfo>> nodes;
  for (auto it = llvm::scc_begin(this); !it.isAtEnd(); ++it) {
    if (it->size() > 1) {
      std::vector<ID<BlockInfo>> blocks;
      for (auto it : *it) {
        blocks.push_back(it->BlockID);
      }

      auto id = blocks[0];
      for (unsigned i = 1, n = blocks.size(); i < n; ++i) {
        id = blocks_.Union(id, blocks[i]);
      }
      nodes.push_back(id);
    } else {
      nodes.push_back(it->at(0)->BlockID);
    }
  }

  // Helper method to propagate nodes.
  // The SCC sorts nodes in reverse topological order - propagate in
  // that direction here, as this graph is directed and acyclic.
  auto PropagateImpl = [&] {
    bool changed = false;
    for (auto it = nodes.rbegin(); it != nodes.rend(); ++it) {
      BlockInfo *node = blocks_.Map(*it);
      for (auto succID : node->Successors) {
        changed |= blocks_.Map(succID)->Taint.Union(node->Taint);
      }
    }
    return changed;
  };

  PropagateImpl();
  llvm::errs() << "DONE\n";
}

// -----------------------------------------------------------------------------
bool TaintedObjects::ExpandIndirect()
{
  bool changed = false;

  // Expand indirect jumps.
  {
    std::set<BlockInfo *> expandedJumps;
    auto indirectJumps = indirectJumps_;
    for (auto &jump : indirectJumps) {
      auto *node = blocks_.Map(jump.From);
      if (!expandedJumps.insert(node).second) {
        continue;
      }
      for (auto blockID : node->Taint.blocks()) {
        auto id = Explore(jump.CS.Indirect(), *graph_[blockID]);
        if (node->Successors.Insert(id)) {
          changed = true;
        }
      }
    }
  }

  // Expand indirect calls.
  {
    std::set<BlockInfo *> expandedCalls;
    auto indirectCalls = indirectCalls_;
    for (auto &call : indirectCalls) {
      auto *node = blocks_.Map(call.From);
      if (!expandedCalls.insert(node).second) {
        continue;
      }

      std::set<BlockInfo *> expandedConts;
      for (auto c : call.Cont) {
        if (!expandedConts.insert(blocks_.Map(c)).second) {
          continue;
        }
        for (auto funcID : node->Taint.funcs()) {
          auto id = Explore(call.CS.Indirect(), *graph_[funcID]);
          for (auto exitID : id.Exit) {
            auto *ret = blocks_.Map(exitID);
            if (node->Successors.Insert(id.Entry)) {
              changed = true;
            }
            if (ret->Successors.Insert(c)) {
              changed = true;
            }
          }
        }
      }
    }
  }

  return changed;
}
