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
Inst *GetCallee(const Inst *inst)
{
  switch (inst->GetKind()) {
    case Inst::Kind::CALL: {
      return static_cast<const CallInst *>(inst)->GetCallee();
    }
    case Inst::Kind::TCALL:
    case Inst::Kind::INVOKE:
    case Inst::Kind::TINVOKE: {
      return static_cast<const CallSite<TerminatorInst> *>(inst)->GetCallee();
    }
    default: {
      return nullptr;
    }
  }
}

// -----------------------------------------------------------------------------
void SimplifyTrampolinePass::Run(Prog *prog)
{
  std::unique_ptr<TrampolineGraph> tg;

  for (auto it = prog->begin(); it != prog->end(); ) {
    Func *caller = &*it++;
    if (auto *callee = GetTarget(caller)) {
      llvm::SmallVector<Inst *, 8> callSites;

      CallingConv cr = caller->GetCallingConv();
      CallingConv ce = callee->GetCallingConv();

      // Check if all calls to the callee can be altered.
      // If both the caller and the callee are C methods, the alteration is
      // trivial. If one of the is OCaml, the alteration is only allowed
      // if every single call site is in an OCaml method.
      bool CanReplace = true;
      for (User *callUser : callee->users()) {
        if (auto *movInst = ::dyn_cast_or_null<MovInst>(callUser)) {
          for (User *movUser : movInst->users()) {
            if (auto *inst = ::dyn_cast_or_null<Inst>(movUser)) {
              Func *siteFunc = inst->getParent()->getParent();
              if (siteFunc == caller) {
                continue;
              }

              Value *t = GetCallee(inst);
              if (!t || t != movInst) {
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
            case Inst::Kind::CALL: {
              static_cast<CallInst *>(call)->SetCallingConv(cr);
              break;
            }
            case Inst::Kind::TCALL:
            case Inst::Kind::INVOKE:
            case Inst::Kind::TINVOKE: {
              static_cast<CallSite<TerminatorInst> *>(call)->SetCallingConv(cr);
              break;
            }
            default:
              llvm_unreachable("invalid instruction kind");
          }
        }

        // If the function is turned into an OCaml function, the calls issued
        // from the newly created one must be annotated with a frame.
        if (cr == CallingConv::CAML && ce != CallingConv::CAML) {
          // Check if any of the callees require trampoline.
          for (Block &block : *callee) {
            for (Inst &inst : block) {
              Inst *t = GetCallee(&inst);
              if (!t) {
                continue;
              }
              if (!tg) {
                tg = std::make_unique<TrampolineGraph>(prog);
              }
              if (tg->NeedsTrampoline(t)) {
                inst.SetAnnot(CAML_FRAME);
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

// -----------------------------------------------------------------------------
Func *SimplifyTrampolinePass::GetTarget(Func *caller)
{
  // Linear control flow.
  if (caller->size() != 1) {
    return nullptr;
  }
  auto *block = &*caller->begin();

  // The function must return the result of a call.
  auto *ret = ::dyn_cast_or_null<ReturnInst>(block->GetTerminator());
  if (!ret) {
    return nullptr;
  }
  auto *call = ::dyn_cast_or_null<CallInst>(ret->GetValue());
  if (!call) {
    return nullptr;
  }

  // Arguments must be forwarded in order.
  unsigned I = 0;
  for (Inst *inst : call->args()) {
    if (auto *arg = ::dyn_cast_or_null<ArgInst>(inst)) {
      if (arg->GetIdx() != I) {
        return nullptr;
      }
      ++I;
      continue;
    }
    return nullptr;
  }

  // Function can have args + move + call +return.
  if (I + 3 != block->size()) {
    return nullptr;
  }

  // Find the callee.
  auto *mov = ::dyn_cast_or_null<MovInst>(call->GetCallee());
  if (!mov) {
    return nullptr;
  }
  auto *callee = ::dyn_cast_or_null<Func>(mov->GetArg());
  if (!callee) {
    return nullptr;
  }

  // Ensure calling conventions are compatible.
  CallingConv calleeConv = callee->GetCallingConv();
  CallingConv callerConv = caller->GetCallingConv();
  if (call->GetCallingConv() != calleeConv) {
    return nullptr;
  }
  if (!CheckCallingConv(callerConv) || !CheckCallingConv(calleeConv)) {
    return nullptr;
  }

  // Ensure functions are compatible.
  if (callee->IsVarArg() || caller->IsVarArg()) {
    return nullptr;
  }
  if (!callee->IsHidden() || !caller->IsHidden()) {
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
  return callee;
}
