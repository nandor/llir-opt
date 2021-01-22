// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include <queue>

#include <llvm/ADT/SCCIterator.h>

#include "core/insts.h"
#include "core/block.h"
#include "core/func.h"
#include "core/analysis/call_graph.h"
#include "core/analysis/reference_graph.h"



// -----------------------------------------------------------------------------
ReferenceGraph::ReferenceGraph(Prog &prog, CallGraph &graph)
  : graph_(graph)
  , built_(false)
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
const ReferenceGraph::Node &ReferenceGraph::operator[](Func &func)
{
  if (!built_) {
    Build();
    built_ = true;
  }
  return *funcToNode_[&func];
}

// -----------------------------------------------------------------------------
enum class RefKind {
  ESCAPES,
  READ,
  WRITE,
  READ_WRITE,
  UNUSED,
};

// -----------------------------------------------------------------------------
static RefKind Classify(const Inst &inst)
{
  std::queue<std::pair<const Inst *, ConstRef<Inst>>> q;
  q.emplace(&inst, nullptr);

  unsigned loadCount = 0;
  unsigned storeCount = 0;
  std::set<const Inst *> vi;
  while (!q.empty()) {
    auto [i, ref] = q.front();
    q.pop();
    if (!vi.insert(i).second) {
      continue;
    }
    switch (i->GetKind()) {
      default: {
        return RefKind::ESCAPES;
      }
      case Inst::Kind::LOAD: {
        loadCount++;
        continue;
      }
      case Inst::Kind::STORE: {
        auto *store = static_cast<const StoreInst *>(i);
        if (store->GetValue() == ref) {
          return RefKind::ESCAPES;
        }
        storeCount++;
        continue;
      }
      case Inst::Kind::MOV:
      case Inst::Kind::ADD:
      case Inst::Kind::SUB:
      case Inst::Kind::PHI: {
        for (const User *user : i->users()) {
          if (auto *inst = ::cast_or_null<const Inst>(user)) {
            q.emplace(inst, i);
          }
        }
        continue;
      }
    }
  }

  if (storeCount && loadCount) {
    return RefKind::READ_WRITE;
  } else if (storeCount) {
    return RefKind::WRITE;
  } else if (loadCount) {
    return RefKind::READ;
  } else {
    return RefKind::UNUSED;
  }
}

// -----------------------------------------------------------------------------
void ReferenceGraph::ExtractReferences(Func &func, Node &node)
{
  for (Block &block : func) {
    for (Inst &inst : block) {
      if (auto *call = ::cast_or_null<CallSite>(&inst)) {
        if (auto *func = call->GetDirectCallee()) {
          if (!Skip(*func)) {
            if (auto it = funcToNode_.find(func); it != funcToNode_.end()) {
              auto &callee = *it->second;
              node.HasIndirectCalls |= callee.HasIndirectCalls;
              node.HasRaise |= callee.HasRaise;
              node.HasBarrier |= callee.HasBarrier;
              for (auto *g : callee.Read) {
                node.Read.insert(g);
              }
              for (auto *g : callee.Written) {
                node.Written.insert(g);
              }
              for (auto *g : callee.Called) {
                node.Called.insert(g);
              }
              for (auto *b : callee.Blocks) {
                node.Blocks.insert(b);
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
          switch (g.GetKind()) {
            case Global::Kind::FUNC: {
              if (HasIndirectUses(mov)) {
                node.Escapes.insert(&g);
              } else {
                node.Called.insert(&static_cast<Func &>(g));
              }
              return;
            }
            case Global::Kind::BLOCK: {
              node.Blocks.insert(&static_cast<Block &>(g));
              return;
            }
            case Global::Kind::EXTERN: {
              node.Escapes.insert(&g);
              return;
            }
            case Global::Kind::ATOM: {
              auto *object = static_cast<Atom &>(g).getParent();
              if (g.getName() == "caml_globals") {
                // Not followed here.
              } else {
                switch (Classify(inst)) {
                  case RefKind::ESCAPES: {
                    node.Escapes.insert(&g);
                    return;
                  }
                  case RefKind::READ: {
                    node.Read.insert(object);
                    return;
                  }
                  case RefKind::WRITE: {
                    node.Written.insert(object);
                    return;
                  }
                  case RefKind::READ_WRITE: {
                    node.Read.insert(object);
                    node.Written.insert(object);
                    return;
                  }
                  case RefKind::UNUSED: {
                    return;
                  }
                }
                llvm_unreachable("invalid classification");
              }
              return;
            }
          }
          llvm_unreachable("invalid global kind");
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
      if (auto *barrier = ::cast_or_null<BarrierInst>(&inst)) {
        node.HasBarrier = true;
        continue;
      }
    }
  }
}
