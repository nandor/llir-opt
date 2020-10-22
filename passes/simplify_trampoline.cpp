// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include "core/block.h"
#include "core/cast.h"
#include "core/func.h"
#include "core/insts.h"
#include "core/prog.h"
#include "passes/inliner/trampoline_graph.h"
#include "passes/simplify_trampoline.h"



// -----------------------------------------------------------------------------
const char *SimplifyTrampolinePass::kPassID = "simplify-trampoline";

// -----------------------------------------------------------------------------
const char *SimplifyTrampolinePass::GetPassName() const
{
  return "Trampoline Simplification";
}

// -----------------------------------------------------------------------------
static bool CheckCallingConv(CallingConv conv)
{
  switch (conv) {
    case CallingConv::C:
    case CallingConv::CAML:
      return true;
    case CallingConv::CAML_ALLOC:
    case CallingConv::CAML_GC:
    case CallingConv::CAML_RAISE:
    case CallingConv::SETJMP:
      return false;
  }
  llvm_unreachable("invalid calling conv");
}

// -----------------------------------------------------------------------------
bool CheckArgs(CallSite *call)
{
  // Arguments must be forwarded in order.
  for (unsigned i = 0, n = call->arg_size(); i < n; ++i) {
    if (Ref<ArgInst> arg = ::cast_or_null<ArgInst>(call->arg(i))) {
      if (arg->GetIdx() != i) {
        return false;
      }
      continue;
    }
    return false;
  }
  return true;
}

// -----------------------------------------------------------------------------
static Ref<Inst> GetForwardedCallee(Func *func)
{
  auto *term = func->begin()->GetTerminator();
  if (auto *call = ::cast_or_null<TailCallInst>(term)) {
    assert(func->size() == 1 && "function should have one basic block");
    if (!CheckArgs(call)) {
      return nullptr;
    }
    // Function can have args + move + tail call.
    if (call->arg_size() + 2 != call->getParent()->size()) {
      return nullptr;
    }
    return call->GetCallee();
  }

  if (auto *call = ::cast_or_null<CallInst>(term)) {
    if (func->size() != 2) {
      return nullptr;
    }
    auto *retBlock = &*std::next(func->begin());
    if (retBlock->size() != 1) {
      return nullptr;
    }
    auto *ret = ::cast_or_null<ReturnInst>(retBlock->GetTerminator());
    if (!ret) {
      return nullptr;
    }

    // The function must return the result of a call, in order.
    for (unsigned i = 0, n = ret->arg_size(); i < n; ++i) {
      Ref<CallInst> argCall = ::cast_or_null<CallInst>(ret->arg(i));
      if (argCall.Get() != call || argCall.Index() != i) {
        return nullptr;
      }
    }

    // Function can have args + move + call.
    if (!CheckArgs(call)) {
      return nullptr;
    }
    if (call->arg_size() + 2 != call->getParent()->size()) {
      return nullptr;
    }
    return call->GetCallee();
  }

  return nullptr;
}

// -----------------------------------------------------------------------------
static Func *GetTarget(Func *caller)
{
  // Find the forwarded callee.
  Ref<Inst> calledInst = GetForwardedCallee(caller);
  if (!calledInst) {
    return nullptr;
  }

  // Find the called function.
  Ref<MovInst> mov = cast_or_null<MovInst>(calledInst);
  if (!mov) {
    return nullptr;
  }
  Ref<Func> callee = cast_or_null<Func>(mov->GetArg());
  if (!callee) {
    return nullptr;
  }

  // Ensure calling conventions are compatible.
  CallingConv calleeConv = callee->GetCallingConv();
  CallingConv callerConv = caller->GetCallingConv();
  if (!CheckCallingConv(callerConv) || !CheckCallingConv(calleeConv)) {
    return nullptr;
  }

  // Ensure functions are compatible.
  if (callee->IsVarArg() || caller->IsVarArg()) {
    return nullptr;
  }
  if (callee->GetVisibility() != caller->GetVisibility()) {
    return nullptr;
  }
  if (callee->params() != caller->params()) {
    return nullptr;
  }
  // Noinline functions are iffy.
  if (callee->IsNoInline()) {
    return nullptr;
  }

  // Candidate for replacement.
  return callee.Get();
}

// -----------------------------------------------------------------------------
void SimplifyTrampolinePass::Run(Prog *prog)
{
  std::unique_ptr<TrampolineGraph> tg;

  for (auto it = prog->begin(); it != prog->end(); ) {
    Func *caller = &*it++;
    if (caller->IsRoot()) {
      continue;
    }

    if (auto *callee = GetTarget(caller)) {
      llvm::SmallVector<Inst *, 8> callSites;

      CallingConv cr = caller->GetCallingConv();
      CallingConv ce = callee->GetCallingConv();

      // Check if all calls to the callee can be altered.
      // If both the caller and the callee are C methods, the alteration is
      // trivial. If one of them is OCaml, the alteration is only allowed
      // if every single call site is in an OCaml method.
      bool CanReplace = true;
      for (User *callUser : callee->users()) {
        if (auto *movInst = ::cast_or_null<MovInst>(callUser)) {
          for (User *movUser : movInst->users()) {
            if (auto *inst = ::cast_or_null<Inst>(movUser)) {
              Func *siteFunc = inst->getParent()->getParent();
              if (siteFunc == caller) {
                continue;
              }

              Ref<Inst> t = GetCalledInst(inst);
              if (!t || t.Get() != movInst) {
                CanReplace = false;
                break;
              }

              auto conv = siteFunc->GetCallingConv();
              if (cr == CallingConv::CAML && ce != cr && conv != CallingConv::CAML) {
                CanReplace = false;
                break;
              }

              callSites.push_back(inst);
              continue;
            }
            CanReplace = false;
            break;
          }
          if (CanReplace) {
            continue;
          }
        }
        CanReplace = false;
        break;
      }

      if (!CanReplace) {
        continue;
      }

      if (ce != cr) {
        // Adjust the calling convention of the function invoked by the
        // trampoline. This requires all the other call sites to be adjusted
        // as well.
        callee->SetCallingConv(cr);
        for (Inst *call : callSites) {
           switch (call->GetKind()) {
            case Inst::Kind::CALL:
            case Inst::Kind::TCALL:
            case Inst::Kind::INVOKE: {
              static_cast<CallInst *>(call)->SetCallingConv(cr);
              break;
            }
            default: {
              llvm_unreachable("invalid instruction kind");
            }
          }
        }

        // If the function is turned into an OCaml function, the calls issued
        // from the newly created one must be annotated with a frame.
        if (cr == CallingConv::CAML && ce != CallingConv::CAML) {
          // Check if any of the callees require trampoline.
          for (Block &block : *callee) {
            for (Inst &inst : block) {
              Ref<Value> t = GetCalledInst(&inst);
              if (!t) {
                continue;
              }
              if (!tg) {
                tg = std::make_unique<TrampolineGraph>(prog);
              }
              if (tg->NeedsTrampoline(t)) {
                // TODO: add debug information.
                inst.SetAnnot<CamlFrame>();
              }
            }
          }
        }
      }

      caller->replaceAllUsesWith(callee);
      caller->eraseFromParent();
    }
  }
}
