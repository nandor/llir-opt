// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include <unordered_set>
#include <stack>

#include <llvm/ADT/SmallVector.h>
#include <llvm/ADT/SmallPtrSet.h>
#include <llvm/ADT/SCCIterator.h>

#include "core/block.h"
#include "core/call_graph.h"
#include "core/cast.h"
#include "core/cfg.h"
#include "core/func.h"
#include "core/insts.h"
#include "core/pass_manager.h"
#include "core/prog.h"
#include "passes/inliner.h"
#include "passes/inliner/inline_helper.h"
#include "passes/inliner/inline_util.h"
#include "passes/inliner/trampoline_graph.h"



// -----------------------------------------------------------------------------
const char *InlinerPass::kPassID = "inliner";



// -----------------------------------------------------------------------------
static std::pair<unsigned, unsigned> CountUses(Func *func)
{
  unsigned dataUses = 0, codeUses = 0;
  for (const User *user : func->users()) {
    if (auto *inst = ::cast_or_null<const Inst>(user)) {
      if (auto *movInst = ::cast_or_null<const MovInst>(inst)) {
        for (const User *movUsers : movInst->users()) {
          codeUses++;
        }
      } else {
        codeUses++;
      }
    } else {
      dataUses++;
    }
  }
  return { dataUses, codeUses };
}

// -----------------------------------------------------------------------------
bool InlinerPass::CheckGlobalCost(Func *callee)
{
  // Do not inline functions which are too large.
  if (callee->size() > 100) {
    return false;
  }
  // Always inline very short functions.
  if (callee->size() <= 2 && callee->inst_size() < 20) {
    return true;
  }
  auto [dataUses, codeUses] = CountUses(callee);
  if ((dataUses != 0) + codeUses > 1 && GetConfig().Opt == OptLevel::Os) {
    // Do not grow code size when optimising for size.
    return false;
  }
  if (codeUses > 1 || dataUses != 0) {
    // Allow inlining regardless the number of data uses.
    // Inline short functions, even if they do not have a single use.
    if (callee->size() != 1 || callee->begin()->size() > 10) {
      // Decide based on the number of new instructions.
      unsigned numCopies = (dataUses ? 1 : 0) + codeUses;
      if (numCopies * callee->inst_size() > 20) {
        return false;
      }
    }
  }
  return true;
}

// -----------------------------------------------------------------------------
void InlinerPass::Run(Prog *prog)
{
  CallGraph graph(*prog);
  TrampolineGraph tg(prog);

  // Since the functions cannot be changed while the call graph is
  // built, identify SCCs and save the topological ordering first.
  std::set<const Func *> inSCC;
  std::vector<Func *> inlineOrder;
  for (auto it = llvm::scc_begin(&graph); !it.isAtEnd(); ++it) {
    // Record nodes in the SCC.
    const std::vector<const CallGraph::Node *> &scc = *it;
    if (scc.size() > 1) {
      for (auto *node : scc) {
        if (auto *f = node->GetCaller()) {
          inSCC.insert(f);
        }
      }
    }
    for (auto *node : scc) {
      if (auto *f = node->GetCaller()) {
        inlineOrder.push_back(f);
      }
    }
  }

  // Inline functions, considering them in topological order.
  for (Func *caller : inlineOrder) {
    for (auto it = caller->begin(); it != caller->end(); ) {
      // Find a call site with a known target outside an SCC.
      auto *call = ::cast_or_null<CallSite>(it->GetTerminator());
      ++it;
      if (!call) {
        continue;
      }
      auto *callee = GetCallee(call);
      if (!callee || inSCC.count(callee)) {
        continue;
      }
      Ref<Inst> target = call->GetCallee();

      // Bail out if illegal or expensive.
      if (!CanInline(caller, callee) || !CheckGlobalCost(callee)) {
        continue;
      }

      // Perform the inlining.
      InlineHelper(call, callee, tg).Inline();

      // If inlining succeeded, remove the dangling call argument.
      if (auto inst = ::cast_or_null<MovInst>(target)) {
        if (inst->use_empty()) {
          inst->eraseFromParent();
        }
      }
      // If callee is dead, delete it.
      if (!callee->IsEntry() && callee->use_empty()) {
        callee->eraseFromParent();
      }
    }
  }
}

// -----------------------------------------------------------------------------
const char *InlinerPass::GetPassName() const
{
  return "Inliner";
}
