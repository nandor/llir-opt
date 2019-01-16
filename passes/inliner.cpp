// This file if part of the genm-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include <llvm/ADT/SmallPtrSet.h>
#include "core/block.h"
#include "core/func.h"
#include "core/prog.h"
#include "core/insts.h"
#include "core/insts_call.h"
#include "passes/inliner.h"



// -----------------------------------------------------------------------------
void InlinerPass::Run(Prog *prog)
{
  for (auto &func : *prog) {
    for (auto &block : func) {
      for (auto &inst : block) {
        Inst *callee = nullptr;
        switch (inst.GetKind()) {
          case Inst::Kind::CALL: {
            callee = static_cast<CallSite<ControlInst>&>(inst).GetCallee();
            break;
          }
          case Inst::Kind::TCALL: {
            callee = static_cast<CallSite<TerminatorInst>&>(inst).GetCallee();
            break;
          }
          default: {
            continue;
          }
        }

        if (!callee->Is(Inst::Kind::MOV)) {
          continue;
        }

        bool singleCall = true;
        for (auto *user : callee->users()) {
          if (user != &inst) {
            singleCall = false;
            break;
          }
        }
        if (!singleCall) {
          continue;
        }

        auto *value = static_cast<MovInst *>(callee)->GetArg();
        if (!value->Is(Value::Kind::GLOBAL)) {
          continue;
        }

        auto *global = static_cast<Global *>(value);
        if (!global->Is(Global::Kind::FUNC)) {
          continue;
        }

        auto *calleeFunc = static_cast<Func *>(global);
        bool singleUse = true;
        for (auto *user : calleeFunc->users()) {
          if (user != callee) {
            singleUse = false;
          }
        }
        if (!singleUse) {
          continue;
        }

        if (inst.IsTerminator()) {
          assert(!"not implemented");
        } else {
          Inline(static_cast<CallInst *>(&inst), calleeFunc);
          break;
        }
      }
    }
  }
}

// -----------------------------------------------------------------------------
const char *InlinerPass::GetPassName() const
{
  return "Inliner";
}

// -----------------------------------------------------------------------------
void InlinerPass::Inline(CallInst *callInst, Func *callee)
{
  auto *block = callInst->getParent();
  auto *contBlock = block->splitBlock(++callInst->getIterator());
}
