// This file if part of the genm-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include <llvm/ADT/SmallVector.h>
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
          callee->eraseFromParent();
          calleeFunc->eraseFromParent();
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
class InlineContext {
public:
  InlineContext(CallInst *call)
    : call_(call)
  {
    for (auto *arg : call->args()) {
      args_.push_back(arg);
    }
  }

  /// Creates a copy of an instruction and tracks them.
  Inst *Clone(Inst *inst)
  {
    if (Inst *dup = Duplicate(inst)) {
      remap_[inst] = dup;
      return dup;
    } else {
      return nullptr;
    }
  }

  /// Maps an instruction.
  Inst *Map(Inst *inst)
  {
    return remap_[inst];
  }

private:
  /// Creates a copy of an instruction.
  Inst *Duplicate(Inst *inst)
  {
    switch (inst->GetKind()) {
      case Inst::Kind::CALL: assert(!"not implemented");
      case Inst::Kind::TCALL: assert(!"not implemented");
      case Inst::Kind::INVOKE: assert(!"not implemented");
      case Inst::Kind::TINVOKE: assert(!"not implemented");
      case Inst::Kind::RET: {
        return nullptr;
      }
      case Inst::Kind::JCC: assert(!"not implemented");
      case Inst::Kind::JI: assert(!"not implemented");
      case Inst::Kind::JMP: assert(!"not implemented");
      case Inst::Kind::SWITCH: assert(!"not implemented");
      case Inst::Kind::TRAP: assert(!"not implemented");
      case Inst::Kind::LD: assert(!"not implemented");
      case Inst::Kind::ST: assert(!"not implemented");
      case Inst::Kind::XCHG: assert(!"not implemented");
      case Inst::Kind::SET: assert(!"not implemented");
      case Inst::Kind::VASTART: assert(!"not implemented");
      case Inst::Kind::ARG: {
        auto *argInst = static_cast<ArgInst *>(inst);
        assert(argInst->GetIdx() < args_.size());
        return args_[argInst->GetIdx()];
      }
      case Inst::Kind::FRAME: assert(!"not implemented");
      case Inst::Kind::SELECT: assert(!"not implemented");
      case Inst::Kind::ABS: assert(!"not implemented");
      case Inst::Kind::NEG: assert(!"not implemented");
      case Inst::Kind::SQRT: assert(!"not implemented");
      case Inst::Kind::SIN: assert(!"not implemented");
      case Inst::Kind::COS: assert(!"not implemented");
      case Inst::Kind::SEXT: assert(!"not implemented");
      case Inst::Kind::ZEXT: assert(!"not implemented");
      case Inst::Kind::FEXT: assert(!"not implemented");
      case Inst::Kind::MOV: {
        auto *movInst = static_cast<MovInst *>(inst);
        return new MovInst(movInst->GetType(), Map(movInst->GetArg()));
      }
      case Inst::Kind::TRUNC: assert(!"not implemented");
      case Inst::Kind::ADD: assert(!"not implemented");
      case Inst::Kind::AND: assert(!"not implemented");
      case Inst::Kind::CMP: assert(!"not implemented");
      case Inst::Kind::DIV: assert(!"not implemented");
      case Inst::Kind::REM: assert(!"not implemented");
      case Inst::Kind::MUL: assert(!"not implemented");
      case Inst::Kind::OR: assert(!"not implemented");
      case Inst::Kind::ROTL: assert(!"not implemented");
      case Inst::Kind::SLL: assert(!"not implemented");
      case Inst::Kind::SRA: assert(!"not implemented");
      case Inst::Kind::SRL: assert(!"not implemented");
      case Inst::Kind::SUB: assert(!"not implemented");
      case Inst::Kind::XOR: assert(!"not implemented");
      case Inst::Kind::POW: assert(!"not implemented");
      case Inst::Kind::COPYSIGN: assert(!"not implemented");
      case Inst::Kind::UADDO: assert(!"not implemented");
      case Inst::Kind::UMULO: assert(!"not implemented");
      case Inst::Kind::UNDEF: assert(!"not implemented");
      case Inst::Kind::PHI: assert(!"not implemented");
    }
  }

  /// Maps a value.
  Value *Map(Value *val)
  {
    switch (val->GetKind()) {
      case Value::Kind::INST:   return Map(static_cast<Inst *>(val));
      case Value::Kind::GLOBAL: return val;
      case Value::Kind::EXPR:   return val;
      case Value::Kind::CONST:  return val;
    }
  }

private:
  /// Call site being inlined.
  CallInst *call_;
  /// Arguments.
  llvm::SmallVector<Inst *, 8> args_;
  /// Mapping from old to new instructions.
  llvm::DenseMap<Inst *, Inst *> remap_;
};

// -----------------------------------------------------------------------------
void InlinerPass::Inline(CallInst *callInst, Func *callee)
{
  auto *block = callInst->getParent();

  // If the callee has a single block, simply copy it over.
  // Otherwise, more work is required to preserve control flow.
  if (&*callee->begin() == &*callee->rbegin()) {
    InlineContext ctx(callInst);
    for (auto &inst : *callee->begin()) {
      if (auto *newInst = ctx.Clone(&inst)) {
        if (!newInst->getParent()) {
          block->AddInst(newInst, callInst);
        }
      } else {
        auto *retInst = static_cast<ReturnInst *>(&inst);
        if (auto *arg = retInst->GetValue()) {
          callInst->replaceAllUsesWith(ctx.Map(arg));
        }
      }
    }
    callInst->eraseFromParent();
  } else {
    assert(!"not implemented");
    // Split the basic block after the call site: the returns of the callee
    // will jump to the block instead. If the function is not void, a PHI
    // node is added in the landing pad to handle incoming values.
    auto *contBlock = block->splitBlock(++callInst->getIterator());
    PhiInst *phi = nullptr;
    if (auto type = callInst->GetType()) {
      phi = new PhiInst(*type);
      contBlock->AddPhi(phi);
      callInst->replaceAllUsesWith(phi);
    }

    // Remove the call - not needed anymore.
    callInst->eraseFromParent();
  }
}
