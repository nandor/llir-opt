// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include <queue>
#include <stack>
#include <unordered_set>

#include <llvm/ADT/SmallVector.h>
#include <llvm/ADT/SmallPtrSet.h>
#include <llvm/ADT/SCCIterator.h>

#include "core/block.h"
#include "core/cast.h"
#include "core/cfg.h"
#include "core/func.h"
#include "core/insts.h"
#include "core/pass_manager.h"
#include "core/prog.h"
#include "core/analysis/call_graph.h"
#include "passes/inliner.h"
#include "passes/inliner/inline_helper.h"
#include "passes/inliner/inline_util.h"
#include "passes/inliner/trampoline_graph.h"



// -----------------------------------------------------------------------------
const char *InlinerPass::kPassID = "inliner";



// -----------------------------------------------------------------------------
static std::pair<unsigned, unsigned> CountUses(const Func &func)
{
  unsigned dataUses = func.IsEntry() ? 1 : 0, codeUses = 0;
  for (const User *user : func.users()) {
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
bool InlinerPass::CheckGlobalCost(const Func &callee)
{
  if (callee.getName() == "realloc") {
    return false;
  }
  // Do not inline functions which are too large.
  if (callee.size() > 100) {
    return false;
  }
  // Always inline very short functions.
  if (callee.size() <= 2 && callee.inst_size() < 20) {
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
    if (callee.size() != 1 || callee.begin()->size() > 10) {
      // Decide based on the number of new instructions.
      unsigned numCopies = (dataUses ? 1 : 0) + codeUses;
      if (numCopies * callee.inst_size() > 20) {
        return false;
      }
    }
  }
  return true;
}

// -----------------------------------------------------------------------------
bool InlinerPass::CheckInitCost(const CallSite &call, const Func &f)
{
  if (f.getName() == "realloc") {
    return false;
  }
  // Always inline functions which are used once.
  auto [data, code] = CountUses(f);
  if (code == 1) {
    return true;
  }
  // Inline very small functions.
  if (f.inst_size() < 20) {
    return true;
  }
  // Inline short functions without increasing code size too much.
  unsigned copies = (data ? 1 : 0) + code;
  if (copies * f.inst_size() < 100) {
    return true;
  }
  return false;
}

// -----------------------------------------------------------------------------
std::set<Func *> FindArticulationPoints(const std::set<Func *> &funcs)
{
  struct Node {
    std::set<Func *> Edges;
    bool Visited;
    unsigned Depth;
    unsigned Low;

    Node() : Visited(false), Depth(0), Low(0) {}
  };

  std::map<Func *, Node> graph;

  for (auto *func : funcs) {
    for (auto &block : *func) {
      auto *call = ::cast_or_null<CallSite>(block.GetTerminator());
      if (!call) {
        continue;
      }
      auto *callee = call->GetDirectCallee();
      if (!callee || !funcs.count(callee)) {
        continue;
      }
      graph[callee].Edges.insert(func);
      graph[func].Edges.insert(callee);
    }
  }

  std::set<Func *> points;
  unsigned v = 0;
  std::function<void(Func *, Func *)> dfs = [&] (Func *func, Func *pred)
  {
    auto &node = graph[func];
    node.Visited = true;
    node.Depth = node.Low = v++;
    unsigned children = 0;
    for (auto *next : node.Edges) {
      if (next == pred) {
        continue;
      }

      auto &nodeNext = graph[next];
      if (nodeNext.Visited) {
        node.Low = std::min(node.Low, nodeNext.Low);
      } else {
        dfs(next, func);
        node.Low = std::min(node.Low, nodeNext.Low);
        if (nodeNext.Low >= node.Depth && pred) {
          points.insert(func);
        }
        ++children;
      }
    }
    if (!pred && node.Edges.size() > 1) {
      points.insert(func);
    }
  };

  dfs(*funcs.begin(), nullptr);
  return points;
}

// -----------------------------------------------------------------------------
bool InlinerPass::Run(Prog &prog)
{
  bool changed = false;

  // Run the necessary analyses.
  CallGraph cg(prog);
  TrampolineGraph tg(&prog);

  // Since the functions cannot be changed while the call graph is
  // built, identify SCCs and save the topological ordering first.
  std::set<const Func *> inSCC;
  std::vector<Func *> inlineOrder;
  for (auto it = llvm::scc_begin(&cg); !it.isAtEnd(); ++it) {
    // Record nodes in the SCC.
    const std::vector<const CallGraph::Node *> &scc = *it;
    if (scc.size() == 1 && scc[0]->IsRecursive()) {
      if (auto *f = scc[0]->GetCaller()) {
        inSCC.insert(f);
      }
    } else if (scc.size() > 1) {
      std::set<Func *> funcs;
      for (auto *node : scc) {
        if (auto *f = node->GetCaller()) {
          funcs.insert(f);
        }
      }
      const auto &points = FindArticulationPoints(funcs);
      for (auto *node : points.empty() ? funcs : points) {
        inSCC.insert(node);
      }
    }
    for (auto *node : scc) {
      if (auto *f = node->GetCaller()) {
        inlineOrder.push_back(f);
      }
    }
  }

  // Inline around the initialisation path.
  auto &cfg = GetConfig();
  if (auto *entry = ::cast_or_null<Func>(prog.GetGlobal(cfg.Entry))) {
    std::queue<Func *> q;
    q.push(entry);
    while (!q.empty()) {
      Func *caller = q.front();
      q.pop();

      bool inlined = false;
      auto it = caller->begin();
      while (it != caller->end()) {
        // Find call instructions with a known call site.
        auto *call = ::cast_or_null<CallSite>(it->GetTerminator());
        if (!call) {
          ++it;
          continue;
        }
        auto mov = ::cast_or_null<MovInst>(call->GetCallee());
        if (!mov) {
          ++it;
          continue;
        }
        auto callee = ::cast_or_null<Func>(mov->GetArg()).Get();
        if (!callee || inSCC.count(callee)) {
          ++it;
          continue;
        }

        // Do not inline if illegal or expensive. If the callee is a method
        // with a single use, it can be assumed it is on the initialisation
        // pass, thus this conservative inlining pass continue with it.
        if (!CanInline(caller, callee) || !CheckInitCost(*call, *callee)) {
          if (callee->use_size() == 1) {
            q.push(callee);
          }
          ++it;
          continue;
        }
        InlineHelper(call, callee, tg).Inline();
        inlined = true;

        if (mov->use_empty()) {
          mov->eraseFromParent();
        }
      }
      if (inlined) {
        caller->RemoveUnreachable();
        changed = true;
      }
    }
  }

  // Inline functions, considering them in topological order.
  std::set<Func *> toDelete;
  for (Func *caller : inlineOrder) {
    // Do not inline if the caller has no uses.
    if (caller->use_empty() && !caller->IsEntry()) {
      toDelete.insert(caller);
      continue;
    }

    bool inlined = false;
    for (auto it = caller->begin(); it != caller->end(); ) {
      // Find a call site with a known target outside an SCC.
      auto *call = ::cast_or_null<CallSite>(it->GetTerminator());
      if (!call) {
        ++it;
        continue;
      }
      auto mov = ::cast_or_null<MovInst>(call->GetCallee());
      if (!mov) {
        ++it;
        continue;
      }
      auto callee = ::cast_or_null<Func>(mov->GetArg()).Get();
      if (!callee || inSCC.count(callee)) {
        ++it;
        continue;
      }

      // Bail out if illegal or expensive.
      if (!CanInline(caller, callee) || !CheckGlobalCost(*callee)) {
        ++it;
        continue;
      }

      // Perform the inlining.
      InlineHelper(call, callee, tg).Inline();
      inlined = true;

      // If callee is dead, delete it.
      if (mov->use_empty()) {
        mov->eraseFromParent();
      }
      if (!callee->IsEntry() && callee->use_empty()) {
        toDelete.insert(callee);
      }
    }
    if (inlined) {
      caller->RemoveUnreachable();
      changed = true;
    }
  }

  // Delete newly dead functions.
  for (auto *func : toDelete) {
    assert(func->use_empty() && "function has uses");
    func->eraseFromParent();
  }

  return changed;
}

// -----------------------------------------------------------------------------
const char *InlinerPass::GetPassName() const
{
  return "Inliner";
}
