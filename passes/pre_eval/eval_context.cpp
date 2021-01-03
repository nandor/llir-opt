// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include <llvm/ADT/PostOrderIterator.h>
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
  for (Block *block : llvm::ReversePostOrderTraversal<Func *>(&func)) {
    Index.emplace(block, Index.size());
  }

  for (auto it = llvm::scc_begin(&func); !it.isAtEnd(); ++it) {
    auto *node = Nodes.emplace_back(std::make_unique<BlockEvalNode>()).get();

    for (Block *block : *it) {
      node->Blocks.push_back(block);
      BlockToNode.emplace(block, node);
    }
    std::sort(
        node->Blocks.begin(),
        node->Blocks.end(),
        [this](Block *a, Block *b) { return Index[a] < Index[b]; }
    );

    // Connect to other nodes & determine whether node is a loop.
    node->Length = it->size();
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
              succNode->Length + it->size()
          );
        }
      }
    }
    node->IsLoop = isLoop;

    // Sort successors by their length.
    auto &succs = node->Succs;
    std::sort(succs.begin(), succs.end(), [](auto *a, auto *b) {
      return a->Length > b->Length;
    });
    succs.erase(std::unique(succs.begin(), succs.end()), succs.end());
  }

  Current = Nodes.rbegin()->get();
}

// -----------------------------------------------------------------------------
bool EvalContext::FindBypassed(
    std::set<BlockEvalNode *> &nodes,
    std::set<SymbolicContext *> &ctx,
    BlockEvalNode *node)
{
  if (node->Context) {
    nodes.insert(node);
    ctx.insert(node->Context.get());
    return true;
  }
  if (Executed.count(node)) {
    return false;
  }

  bool bypassed = false;
  for (BlockEvalNode *pred : node->Preds) {
    bypassed = FindBypassed(nodes, ctx, pred) || bypassed;
  }
  if (bypassed) {
    nodes.insert(node);
  }
  return bypassed;
}
