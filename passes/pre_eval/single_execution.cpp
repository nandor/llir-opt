// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include <llvm/ADT/SCCIterator.h>

#include "core/block.h"
#include "core/call_graph.h"
#include "core/cfg.h"
#include "core/func.h"
#include "core/inst.h"
#include "core/insts.h"
#include "passes/pre_eval/single_execution.h"



// -----------------------------------------------------------------------------
SingleExecution::SingleExecution(Func &f)
  : f_(f)
  , g_(*f.getParent())
{
}

// -----------------------------------------------------------------------------
std::set<const Block *> SingleExecution::Solve()
{
  // Mark blocks reachable from SCCs in the call graph as members of loops.
  for (auto it = llvm::scc_begin(&g_); !it.isAtEnd(); ++it) {
    if (it->size() == 1) {
      auto *node = (*it)[0];
      if (node->IsRecursive()) {
        MarkInLoop(node);
      }
    } else {
      for (auto &node : *it) {
        if (node->IsRecursive()) {
          MarkInLoop(node);
        }
      }
    }
  }

  // Start from main and mark blocks which were not yet visited.
  Visit(f_);
  for (const Block *b : inLoop_) {
    singleExec_.erase(b);
  }
  return singleExec_;
}

// -----------------------------------------------------------------------------
void SingleExecution::MarkInLoop(const CallGraph::Node *node)
{
  if (auto *f = node->GetCaller()) {
    if (inLoop_.count(&f->getEntryBlock())) {
      return;
    }
    for (const Block &block : *f) {
      inLoop_.insert(&block);
    }
  }

  for (auto *callee : *node) {
    MarkInLoop(callee);
  }
}

// -----------------------------------------------------------------------------
void SingleExecution::MarkInLoop(Block *block)
{
  if (!inLoop_.insert(block).second) {
    return;
  }
  for (Inst &inst : *block) {
    if (auto *call = ::cast_or_null<CallSite>(&inst)) {
      if (auto *f = call->GetDirectCallee()) {
        MarkInLoop(g_[f]);
      }
    }
  }
}

// -----------------------------------------------------------------------------
void SingleExecution::Visit(Func &f)
{
  for (auto it = llvm::scc_begin(&f); !it.isAtEnd(); ++it) {
    if (it->size() != 1) {
      for (Block *block : *it) {
        MarkInLoop(block);
      }
    } else {
      Block *block = *it->begin();
      if (singleExec_.insert(block).second) {
        for (Inst &inst : *block) {
          if (auto *call = ::cast_or_null<CallSite>(&inst)) {
            if (auto *f = call->GetDirectCallee()) {
              Visit(*f);
            }
          }
        }
      } else {
        MarkInLoop(block);
      }
    }
  }
}
