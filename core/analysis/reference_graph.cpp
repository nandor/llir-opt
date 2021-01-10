// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include <queue>

#include <llvm/ADT/SCCIterator.h>

#include "core/call_graph.h"
#include "core/insts.h"
#include "core/block.h"
#include "core/func.h"
#include "core/analysis/reference_graph.h"



// -----------------------------------------------------------------------------
ReferenceGraph::ReferenceGraph(Prog &prog, CallGraph &graph)
  : graph_(graph)
{
}

// -----------------------------------------------------------------------------
void ReferenceGraph::Build()
{
  for (auto it = llvm::scc_begin(&graph_); !it.isAtEnd(); ++it) {
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
          if (Skip(*func)) {
            // Do not follow allocations.
          } else {
            if (auto it = funcToNode_.find(func); it != funcToNode_.end()) {
              auto &callee = *it->second;
              node.HasIndirectCalls |= callee.HasIndirectCalls;
              node.HasRaise |= callee.HasRaise;
              for (auto *g : callee.Referenced) {
                node.Referenced.insert(g);
              }
               for (auto *g : callee.Called) {
                node.Called.insert(g);
              }
            }
          }
        } else {
          node.HasIndirectCalls = true;
        }
        continue;
      }
      if (auto *mov = ::cast_or_null<MovInst>(&inst)) {
        auto extract = [&](Global &g)
        {
          if (auto *f = ::cast_or_null<Func>(&g)) {
            if (HasIndirectUses(mov)) {
              node.Referenced.insert(&g);
            } else {
              node.Called.insert(f);
            }
          } else {
            if (g.getName() == "caml_globals") {
              // Not followed here.
            } else {
              node.Referenced.insert(&g);
            }
          }
        };

        auto movArg = mov->GetArg();
        switch (movArg->GetKind()) {
          case Value::Kind::GLOBAL: {
            extract(*::cast<Global>(movArg));
            continue;
          }
          case Value::Kind::EXPR: {
            switch (::cast<Expr>(movArg)->GetKind()) {
              case Expr::Kind::SYMBOL_OFFSET: {
                auto symExpr = ::cast<SymbolOffsetExpr>(movArg);
                extract(*symExpr->GetSymbol());
                continue;
              }
            }
            llvm_unreachable("invalid expression kind");
          }
          case Value::Kind::INST:
          case Value::Kind::CONST: {
            continue;
          }
        }
        llvm_unreachable("invalid value kind");
      }
      if (auto *raise = ::cast_or_null<RaiseInst>(&inst)) {
        node.HasRaise = true;
        continue;
      }
    }
  }
}
