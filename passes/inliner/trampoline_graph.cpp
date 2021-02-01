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
static const char *kAllocSites[] = {
  "caml_stat_alloc_noexc",
  "caml_stat_resize_noexc",
};

// -----------------------------------------------------------------------------
static bool IsAllocation(const std::string_view name)
{
  for (const char *tramp : kAllocSites) {
    if (tramp == name) {
      return true;
    }
  }
  return false;
}

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
bool TrampolineGraph::NeedsTrampoline(ConstRef<Value> callee)
{
  if (ConstRef<MovInst> movInstRef = ::cast_or_null<MovInst>(callee)) {
    auto movVal = movInstRef->GetArg();
    switch (movVal->GetKind()) {
      case Value::Kind::INST: {
        return true;
      }
      case Value::Kind::GLOBAL: {
        auto &g = *cast<Global>(movVal);
        switch (g.GetKind()) {
          case Global::Kind::EXTERN: {
            return true;
          }
          case Global::Kind::FUNC: {
            auto &func = static_cast<const Func &>(g);
            switch (func.GetCallingConv()) {
              case CallingConv::C: {
                return graph_[&func].Trampoline;
              }
              case CallingConv::CAML:
              case CallingConv::CAML_ALLOC:
              case CallingConv::CAML_GC: {
                return true;
              }
              case CallingConv::SETJMP:
              case CallingConv::XEN:
              case CallingConv::INTR: {
                return false;
              }
            }
            llvm_unreachable("invalid calling convention");
          }
          case Global::Kind::BLOCK:
          case Global::Kind::ATOM: {
            llvm_unreachable("invalid call target");
          }
        }
        llvm_unreachable("invalid global kind");
      }
      case Value::Kind::EXPR:
      case Value::Kind::CONST: {
        return false;
      }
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
        case CallingConv::XEN:
        case CallingConv::INTR: {
          break;
        }
        case CallingConv::CAML:
        case CallingConv::CAML_ALLOC:
        case CallingConv::CAML_GC: {
          continue;
        }
      }

      // Look for callees - indirect call sites and allocators need trampolines.
      for (const Inst &inst : block) {
        switch (inst.GetKind()) {
          case Inst::Kind::CALL:
          case Inst::Kind::TAIL_CALL:
          case Inst::Kind::INVOKE: {
            auto callee = static_cast<const CallSite &>(inst).GetCallee();
            if (ConstRef<MovInst> inst = ::cast_or_null<MovInst>(callee)) {
              ConstRef<Value> movVal = inst->GetArg();
              switch (movVal->GetKind()) {
                case Value::Kind::INST: {
                  break;
                }
                case Value::Kind::GLOBAL: {
                  const Global &g = *::cast<Global>(movVal);
                  switch (g.GetKind()) {
                    case Global::Kind::EXTERN: {
                      graph_[&func].Trampoline = true;
                      continue;
                    }
                    case Global::Kind::FUNC: {
                      if (IsAllocation(g.GetName())) {
                        graph_[&func].Trampoline = true;
                      } else {
                        graph_[&func].Out.insert(&static_cast<const Func &>(g));
                      }
                      continue;
                    }
                    case Global::Kind::BLOCK:
                    case Global::Kind::ATOM: {
                      llvm_unreachable("invalid call target");
                    }
                  }
                  break;
                }
                case Value::Kind::EXPR:
                case Value::Kind::CONST: {
                  llvm_unreachable("invalid call target");
                }
              }
            }
            graph_[&func].Trampoline = true;
            break;
          }
          case Inst::Kind::RAISE: {
            graph_[&func].Trampoline = true;
            continue;
          }
          default: {
            continue;
          }
        }
      }
    }
  }
}

// -----------------------------------------------------------------------------
void TrampolineGraph::Visit(const Func *func)
{
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
