// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include <llvm/ADT/SCCIterator.h>

#include "core/block.h"
#include "core/cfg.h"
#include "core/func.h"
#include "passes/pre_eval/eval_context.h"



// -----------------------------------------------------------------------------
bool BlockEvalNode::IsReturn() const
{
  for (Block *block : Blocks) {
    if (block->GetTerminator()->IsReturn()) {
      return true;
    }
  }
  return false;
}

// -----------------------------------------------------------------------------
llvm::raw_ostream &operator<<(llvm::raw_ostream &os, BlockEvalNode &node)
{
  bool first = true;
  for (Block *block :node.Blocks) {
    if (!first) {
      os << ", ";
    }
    first = false;
    os << block->getName();
  }
  return os;
}

// -----------------------------------------------------------------------------
EvalContext::EvalContext(Func &func)
  : func_(func)
{
  for (auto it = llvm::scc_begin(&func); !it.isAtEnd(); ++it) {
    auto *node = Nodes.emplace_back(std::make_unique<BlockEvalNode>()).get();

    unsigned size = 0;
    for (Block *block : *it) {
      node->Blocks.insert(block);
      BlockToNode.emplace(block, node);
      size += block->size();
    }

    // Connect to other nodes & determine whether node is a loop.
    node->Length = size;
    node->Returns = node->IsReturn();
    bool isLoop = it->size() > 1;
    for (Block *block :*it) {
      for (Block *succ : block->successors()) {
        auto *succNode = BlockToNode[succ];
        if (succNode == node) {
          isLoop = true;
        } else {
          node->Succs.push_back(succNode);
          succNode->Preds.insert(node);
          node->Length = std::max(
              node->Length,
              succNode->Length + size
          );
          node->Returns = node->Returns || succNode->Returns;
        }
      }
    }
    node->IsLoop = isLoop;

    // Sort successors by their length.
    auto &succs = node->Succs;
    std::sort(succs.begin(), succs.end(), [](auto *a, auto *b) {
      if (a->Returns == b->Returns) {
        return a->Length > b->Length;
      } else {
        return a->Returns;
      }
    });
    succs.erase(std::unique(succs.begin(), succs.end()), succs.end());
  }

  Current = Nodes.rbegin()->get();
}

// -----------------------------------------------------------------------------
bool EvalContext::FindBypassed(
    std::set<BlockEvalNode *> &nodes,
    std::set<SymbolicContext *> &ctx,
    BlockEvalNode *start,
    BlockEvalNode *end)
{
  if (start->Context) {
    nodes.insert(start);
    ctx.insert(start->Context.get());
    return true;
  }
  if (ExecutedNodes.count(start)) {
    return false;
  }

  bool bypassed = false;
  for (BlockEvalNode *pred : start->Preds) {
    bypassed = FindBypassed(nodes, ctx, pred, start) || bypassed;
  }
  if (bypassed) {
    nodes.insert(start);
  }
  return bypassed;
}

// -----------------------------------------------------------------------------
bool EvalContext::IsActive(Block *from, BlockEvalNode *node)
{
  auto *fromNode = BlockToNode[from];
  if (Approximated.count(fromNode)) {
    return true;
  }
  return ExecutedEdges.count(std::make_pair(fromNode, node));
}
