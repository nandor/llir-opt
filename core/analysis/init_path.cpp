// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include <stack>

#include <llvm/ADT/SCCIterator.h>

#include "core/cfg.h"
#include "core/dag.h"
#include "core/block.h"
#include "core/func.h"
#include "core/prog.h"
#include "core/analysis/call_graph.h"
#include "core/analysis/init_path.h"



// -----------------------------------------------------------------------------
static bool IsSingleUse(const Func &func)
{
  if (!func.IsLocal()) {
    return false;
  }
  unsigned codeUses = 0;
  for (const User *user : func.users()) {
    if (auto *inst = ::cast_or_null<const MovInst>(user)) {
      for (const User *movUsers : inst->users()) {
        codeUses++;
      }
    } else {
      return false;
    }
  }
  return codeUses == 1;
}

// -----------------------------------------------------------------------------
InitPath::InitPath(Prog &prog, Func *entry)
{
  std::stack<std::pair<std::unique_ptr<DAGFunc>, DAGFunc::reverse_node_iterator>> stk;
  if (entry) {
    auto *dag = new DAGFunc(*entry);
    stk.emplace(dag, dag->rbegin());
  }

  while (!stk.empty()) {
    auto &[dag, it] = stk.top();
    if (it == dag->rend()) {
      stk.pop();
      continue;
    }

    auto *node = *it++;
    if (!node->IsLoop) {
      auto *block = *node->Blocks.begin();
      executedAtMostOnce_.insert(block);
      if (auto *call = ::cast_or_null<CallSite>(block->GetTerminator())) {
        if (auto *f = call->GetDirectCallee(); f && IsSingleUse(*f)) {
          auto *dag = new DAGFunc(*f);
          stk.emplace(dag, dag->rbegin());
          continue;
        }
      }
    }
  }
}
