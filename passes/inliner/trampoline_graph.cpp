// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include "core/calling_conv.h"
#include "core/cast.h"
#include "core/func.h"
#include "core/insts.h"
#include "core/prog.h"
#include "core/visibility.h"
#include "passes/inliner/trampoline_graph.h"



// -----------------------------------------------------------------------------
TrampolineGraph::TrampolineGraph(const Prog *prog)
{
  BuildGraph(prog);

  for (const Func &f : *prog) {
    auto it = graph_.find(&f);
    if (it != graph_.end() && it->second.Index == 0) {
      Visit(&f);
    }
  }
}

// -----------------------------------------------------------------------------
bool TrampolineGraph::NeedsTrampoline(const Value *callee)
{
  if (auto *movInst = ::dyn_cast_or_null<const MovInst>(callee)) {
    auto *movVal = movInst->GetArg();
    switch (movVal->GetKind()) {
      case Value::Kind::INST:
        return true;
      case Value::Kind::GLOBAL:
        switch (static_cast<Global *>(movVal)->GetKind()) {
          case Global::Kind::EXTERN:
            return false;
          case Global::Kind::FUNC: {
            auto *func = static_cast<Func *>(movVal);
            switch (func->GetCallingConv()) {
              case CallingConv::C:
                return graph_[func].Trampoline;
              case CallingConv::CAML:
              case CallingConv::CAML_ALLOC:
              case CallingConv::CAML_GC:
              case CallingConv::CAML_RAISE:
                return true;
              case CallingConv::SETJMP:
                return false;
            }
          }
          case Global::Kind::BLOCK:
          case Global::Kind::ATOM:
            llvm_unreachable("invalid call target");
        }
        llvm_unreachable("invalid global kind");
      case Value::Kind::EXPR:
      case Value::Kind::CONST:
        llvm_unreachable("invalid call target");
    }
    llvm_unreachable("invalid value kind");
  }
  return true;
}

// -----------------------------------------------------------------------------
void TrampolineGraph::BuildGraph(const Prog *prog)
{
  for (const Func &func : *prog) {
    for (const Block &block : func) {
      // Start building the graph at C call sites.
      switch (func.GetCallingConv()) {
        case CallingConv::C:
        case CallingConv::SETJMP:
          break;
        case CallingConv::CAML:
        case CallingConv::CAML_ALLOC:
        case CallingConv::CAML_GC:
        case CallingConv::CAML_RAISE:
          continue;
      }

      // Look for callees - indirect call sites and allocators need trampolines.
      for (const Inst &inst : block) {
        const Value *callee;
        switch (inst.GetKind()) {
          case Inst::Kind::CALL:
            callee = static_cast<const CallSite<Inst> *>(&inst)->GetCallee();
            break;
          case Inst::Kind::TCALL:
          case Inst::Kind::INVOKE:
          case Inst::Kind::TINVOKE:
            callee = static_cast<const CallSite<TerminatorInst> *>(&inst)->GetCallee();
            break;
          case Inst::Kind::JI:
            graph_[&func].Trampoline = true;
            continue;
          default:
            continue;
        }

        if (auto *movInst = ::dyn_cast_or_null<const MovInst>(callee)) {
          auto *movVal = movInst->GetArg();
          switch (movVal->GetKind()) {
            case Value::Kind::INST:
              break;
            case Value::Kind::GLOBAL:
              switch (static_cast<const Global *>(movVal)->GetKind()) {
                case Global::Kind::EXTERN:
                  break;
                case Global::Kind::FUNC: {
                  static const char *tramps[] = {
                    "caml_stat_alloc_noexc",
                    "caml_stat_resize_noexc",
                    "caml_raise",
                  };
                  bool needsTrampoline = false;
                  for (const char *tramp : tramps) {
                    if (tramp == func.GetName()) {
                      needsTrampoline = true;
                      break;
                    }
                  }
                  if (needsTrampoline) {
                    graph_[&func].Trampoline = true;
                  } else {
                    graph_[&func].Out.insert(static_cast<const Func *>(movVal));
                  }
                  continue;
                }
                case Global::Kind::BLOCK:
                case Global::Kind::ATOM:
                  llvm_unreachable("invalid call target");
              }
              break;
            case Value::Kind::EXPR:
            case Value::Kind::CONST:
              llvm_unreachable("invalid call target");
          }
        }

        graph_[&func].Trampoline = true;
      }
    }
  }
}

// -----------------------------------------------------------------------------
void TrampolineGraph::Visit(const Func *func) {
  Node &node = graph_[func];
  node.Index = index_;
  node.LowLink = index_;
  index_++;
  stack_.push(func);
  node.OnStack = true;

  for (const Func *w : node.Out) {
    auto &nodeW = graph_[w];
    if (nodeW.Index == 0) {
      Visit(w);
      node.LowLink = std::min(node.LowLink, nodeW.LowLink);
    } else if (nodeW.OnStack) {
      node.LowLink = std::min(node.LowLink, nodeW.LowLink);
    }
  }

  if (node.LowLink == node.Index) {
    std::vector<const Func *> scc{ func };

    const Func *w;
    do {
      w = stack_.top();
      scc.push_back(w);
      stack_.pop();
      graph_[w].OnStack = false;
    } while (w != func);

    bool needsTrampoline = false;
    for (const Func *w : scc) {
      auto &node = graph_[w];
      if (node.Trampoline) {
        needsTrampoline = true;
        break;
      }
      for (const Func *v : node.Out) {
        if (graph_[v].Trampoline) {
          needsTrampoline = true;
          break;
        }
      }
      if (needsTrampoline) {
        break;
      }
    }

    if (needsTrampoline) {
      for (const Func *w : scc) {
        graph_[w].Trampoline = true;
      }
    }
  }
}
