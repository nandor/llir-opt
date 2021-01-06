// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include <queue>

#include <llvm/ADT/SCCIterator.h>

#include "core/call_graph.h"
#include "core/insts.h"
#include "core/block.h"
#include "core/func.h"
#include "passes/pre_eval/reference_graph.h"



// -----------------------------------------------------------------------------
ReferenceGraph::ReferenceGraph(Prog &prog, CallGraph &graph)
  : graph_(graph)
{
  for (auto it = llvm::scc_begin(&graph); !it.isAtEnd(); ++it) {
    auto &node = *nodes_.emplace_back(std::make_unique<Node>());
    for (auto *sccNode : *it) {
      if (auto *func = sccNode->GetCaller()) {
        ExtractReferences(*func, node);
      }
    }
    for (auto *sccNode : *it) {
      if (auto *func = sccNode->GetCaller()) {
        funcToNode_.emplace(func, &node);
      }
    }
  }
}

// -----------------------------------------------------------------------------
static bool HasIndirectUses(MovInst *inst)
{
  std::queue<MovInst *> q;
  q.push(inst);
  while (!q.empty()) {
    MovInst *i = q.front();
    q.pop();
    for (User *user : i->users()) {
      if (auto *mov = ::cast_or_null<MovInst>(user)) {
        q.push(mov);
      } else if (auto *call = ::cast_or_null<CallSite>(user)) {
        if (call->GetCallee().Get() != i) {
          return true;
        }
      } else {
        return true;
      }
    }
  }
  return false;
}

// -----------------------------------------------------------------------------
void ReferenceGraph::ExtractReferences(Func &func, Node &node)
{
  for (Block &block : func) {
    for (Inst &inst : block) {
      if (auto *call = ::cast_or_null<CallSite>(&inst)) {
        if (auto *func = call->GetDirectCallee()) {
          if (func->getName() == "caml_alloc_shr") {

          } else if (func->getName() == "caml_check_urgent_gc") {

          } else if (func->getName() == "caml_alloc_shr_aux") {

          } else if (func->getName() == "caml_alloc_small_aux") {

          } else {
            if (auto it = funcToNode_.find(func); it != funcToNode_.end()) {
              auto &callee = *it->second;
              node.HasIndirectCalls |= callee.HasIndirectCalls;
              node.HasRaise |= callee.HasRaise;
              for (auto *g : callee.Referenced) {
                node.Referenced.insert(g);
              }
            }
          }
        } else {
          node.HasIndirectCalls = true;
        }
        continue;
      }
      if (auto *mov = ::cast_or_null<MovInst>(&inst)) {
        if (auto g = ::cast_or_null<Global>(mov->GetArg())) {
          if (g->Is(Global::Kind::FUNC)) {
            if (HasIndirectUses(mov)) {
              node.Referenced.insert(&*g);
            }
          } else {
            if (g->getName() == "caml_globals") {
              continue;
            }
            node.Referenced.insert(&*g);
          }
        }
        continue;
      }
      if (auto *raise = ::cast_or_null<RaiseInst>(&inst)) {
        node.HasRaise = true;
        continue;
      }
    }
  }
}
