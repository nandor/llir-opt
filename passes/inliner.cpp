// This file if part of the genm-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include <sstream>

#include <llvm/ADT/PostOrderIterator.h>
#include <llvm/ADT/SmallVector.h>
#include <llvm/ADT/SmallPtrSet.h>

#include "core/block.h"
#include "core/cfg.h"
#include "core/func.h"
#include "core/insts.h"
#include "core/insts_binary.h"
#include "core/insts_call.h"
#include "core/insts_control.h"
#include "core/insts_memory.h"
#include "core/prog.h"
#include "passes/inliner.h"



// -----------------------------------------------------------------------------
class InlineContext {
public:
  InlineContext(CallInst *call, Func *callee)
    : call_(call)
    , entry_(call->getParent())
    , callee_(callee)
    , caller_(entry_->getParent())
    , stackSize_(caller_->GetStackSize())
    , exit_(nullptr)
    , phi_(nullptr)
    , numExits_(0)
    , rpot_(callee_)
  {
    // Adjust the caller's stack.
    auto *caller = entry_->getParent();
    if (unsigned stackSize = callee->GetStackSize()) {
      caller->SetStackSize(stackSize_ + stackSize);
    }

    // Prepare the arguments.
    for (auto *arg : call->args()) {
      args_.push_back(arg);
    }

    // Checks if the function has a single exit.
    unsigned numBlocks = 0;
    for (auto *block : rpot_) {
      numBlocks++;
      if (block->succ_empty()) {
        numExits_++;
      }
    }

    // Split the block if the callee's CFG has multiple blocks.
    if (numBlocks > 1) {
      exit_ = entry_->splitBlock(++call_->getIterator());
    }

    if (numExits_ > 1) {
      if (auto type = call_->GetType()) {
        phi_ = new PhiInst(*type);
        exit_->AddPhi(phi_);
        call_->replaceAllUsesWith(phi_);
      }
      call_->eraseFromParent();
      call_ = nullptr;
    }

    // Handle all reachable blocks.
    auto *after = entry_;
    for (auto &block : rpot_) {
      if (block == &callee->getEntryBlock()) {
        blocks_[block] = entry_;
        continue;
      }
      if (block->succ_empty() && numExits_ <= 1) {
        blocks_[block] = exit_;
        continue;
      }

      std::ostringstream os;
      os << block->GetName() << "$" << caller->GetName();
      auto *newBlock = new Block(caller, os.str());
      caller->insertAfter(after->getIterator(), newBlock);
      after = newBlock;
      blocks_[block] = newBlock;
    }
  }

  /// Inlines the function.
  void Inline()
  {
    // Inline all blocks from the callee.
    for (auto *block : rpot_) {
      auto *target = Map(block);
      Inst *insert;
      if (target->IsEmpty()) {
        insert = nullptr;
      } else {
        if (target == entry_) {
          insert = exit_ ? nullptr : call_;
        } else {
          insert = &*target->begin();
        }
      }

      for (auto &inst : *block) {
        Clone(target, insert, &inst);
      }
    }

    // Fix up PHIs.
    for (auto &phi : phis_) {
      auto *phiInst = phi.first;
      auto *phiNew = phi.second;
      for (unsigned i = 0; i < phiInst->GetNumIncoming(); ++i) {
        phiNew->Add(Map(phiInst->GetBlock(i)), Map(phiInst->GetValue(i)));
      }
    }
  }

private:
  /// Creates a copy of an instruction and tracks them.
  Inst *Clone(Block *block, Inst *before, Inst *inst)
  {
    if (Inst *dup = Duplicate(block, before, inst)) {
      insts_[inst] = dup;
      return dup;
    } else {
      return nullptr;
    }
  }

  /// Creates a copy of an instruction.
  Inst *Duplicate(Block *block, Inst *before, Inst *inst)
  {
    auto add = [block, before] (Inst *inst) {
      block->AddInst(inst, before);
      return inst;
    };

    switch (inst->GetKind()) {
      case Inst::Kind::CALL: {
        auto *callInst = static_cast<CallInst *>(inst);
        std::vector<Inst *> args;
        for (auto *arg : callInst->args()) {
          args.push_back(Map(arg));
        }
        return add(new CallInst(
            callInst->GetType(),
            Map(callInst->GetCallee()),
            args,
            callInst->GetNumFixedArgs(),
            callInst->GetCallingConv(),
            callInst->GetAnnotation()
        ));
      }
      case Inst::Kind::TCALL: {
        // Convert the tail call to a call and jump to the landing pad.
        auto *callInst = static_cast<TailCallInst *>(inst);
        std::vector<Inst *> args;
        for (auto *arg : callInst->args()) {
          args.push_back(Map(arg));
        }

        auto *value = add(new CallInst(
            callInst->GetType(),
            Map(callInst->GetCallee()),
            args,
            callInst->GetNumFixedArgs(),
            callInst->GetCallingConv(),
            callInst->GetAnnotation()
        ));

        if (callInst->GetType()) {
          if (phi_) {
            phi_->Add(block, value);
          } else {
            call_->replaceAllUsesWith(value);
          }
        }
        if (numExits_ > 1) {
          block->AddInst(new JumpInst(exit_));
        }
        if (call_) {
          call_->eraseFromParent();
        }

        return nullptr;
      }
      case Inst::Kind::INVOKE: assert(!"not implemented");
      case Inst::Kind::TINVOKE: assert(!"not implemented");
      case Inst::Kind::RET: {
        // Add the returned value to the PHI if one was generated.
        // If there are multiple returns, add a jump to the target.
        auto *retInst = static_cast<ReturnInst *>(inst);
        if (auto *val = retInst->GetValue()) {
          if (phi_) {
            phi_->Add(block, Map(val));
          } else {
            call_->replaceAllUsesWith(Map(val));
          }
        }
        if (numExits_ > 1) {
          block->AddInst(new JumpInst(exit_));
        }
        if (call_) {
          call_->eraseFromParent();
        }
        return nullptr;
      }
      case Inst::Kind::JCC: {
        auto *jccInst = static_cast<JumpCondInst *>(inst);
        return add(new JumpCondInst(
            Map(jccInst->GetCond()),
            Map(jccInst->GetTrueTarget()),
            Map(jccInst->GetFalseTarget())
        ));
      }
      case Inst::Kind::JI: assert(!"not implemented");
      case Inst::Kind::JMP: {
        auto *jmpInst = static_cast<JumpInst *>(inst);
        return add(new JumpInst(Map(jmpInst->GetTarget())));
      }
      case Inst::Kind::SWITCH: {
        auto *switchInst = static_cast<SwitchInst *>(inst);
        std::vector<Block *> branches;
        for (unsigned i = 0; i < switchInst->getNumSuccessors(); ++i) {
          branches.push_back(Map(switchInst->getSuccessor(i)));
        }
        return add(new SwitchInst(Map(switchInst->GetIdx()), branches));
      }
      case Inst::Kind::TRAP: {
        // Erase everything after the inlined trap instruction.
        auto *trapNew = add(new TrapInst());
        if (call_) {
          block->erase(before->getIterator(), block->end());
        }
        return trapNew;
      }
      case Inst::Kind::LD: {
        auto *loadInst = static_cast<LoadInst *>(inst);
        return add(new LoadInst(
            loadInst->GetLoadSize(),
            loadInst->GetType(),
            Map(loadInst->GetAddr())
        ));
      }
      case Inst::Kind::ST: {
        auto *storeInst = static_cast<StoreInst *>(inst);
        return add(new StoreInst(
            storeInst->GetStoreSize(),
            Map(storeInst->GetAddr()),
            Map(storeInst->GetVal())
        ));
      }
      case Inst::Kind::XCHG: assert(!"not implemented");
      case Inst::Kind::SET: assert(!"not implemented");
      case Inst::Kind::VASTART: assert(!"not implemented");
      case Inst::Kind::ARG: {
        auto *argInst = static_cast<ArgInst *>(inst);
        assert(argInst->GetIdx() < args_.size());
        return args_[argInst->GetIdx()];
      }
      case Inst::Kind::FRAME: {
        auto *frameInst = static_cast<FrameInst *>(inst);
        return add(new FrameInst(
            frameInst->GetType(),
            new ConstantInt(frameInst->GetIdx() + stackSize_)
        ));
      }
      case Inst::Kind::SELECT: {
        auto *selectInst = static_cast<SelectInst *>(inst);
        return add(new SelectInst(
            selectInst->GetType(),
            Map(selectInst->GetCond()),
            Map(selectInst->GetTrue()),
            Map(selectInst->GetFalse())
        ));
      }
      case Inst::Kind::MOV: {
        auto *movInst = static_cast<MovInst *>(inst);
        return add(new MovInst(
            movInst->GetType(),
            Map(movInst->GetArg())
        ));
      }
      case Inst::Kind::CMP: {
        auto *cmpInst = static_cast<CmpInst *>(inst);
        return add(new CmpInst(
            cmpInst->GetType(),
            cmpInst->GetCC(),
            Map(cmpInst->GetLHS()),
            Map(cmpInst->GetRHS())
        ));
      }
      case Inst::Kind::ABS:
      case Inst::Kind::NEG:
      case Inst::Kind::SQRT:
      case Inst::Kind::SIN:
      case Inst::Kind::COS:
      case Inst::Kind::SEXT:
      case Inst::Kind::ZEXT:
      case Inst::Kind::FEXT:
      case Inst::Kind::TRUNC: {
        auto *unaryInst = static_cast<UnaryInst *>(inst);
        return add(new UnaryInst(
            unaryInst->GetKind(),
            unaryInst->GetType(),
            Map(unaryInst->GetArg())
        ));
      }

      case Inst::Kind::ADD:
      case Inst::Kind::AND:
      case Inst::Kind::DIV:
      case Inst::Kind::REM:
      case Inst::Kind::MUL:
      case Inst::Kind::OR:
      case Inst::Kind::ROTL:
      case Inst::Kind::SLL:
      case Inst::Kind::SRA:
      case Inst::Kind::SRL:
      case Inst::Kind::SUB:
      case Inst::Kind::XOR:
      case Inst::Kind::POW: {
        auto *binInst = static_cast<BinaryInst *>(inst);
        return add(new BinaryInst(
            binInst->GetKind(),
            binInst->GetType(),
            Map(binInst->GetLHS()),
            Map(binInst->GetRHS())
        ));
      }
      case Inst::Kind::COPYSIGN: assert(!"not implemented");
      case Inst::Kind::UADDO:
      case Inst::Kind::UMULO: {
        auto *ovInst = static_cast<OverflowInst *>(inst);
        return add(new OverflowInst(
            ovInst->GetKind(),
            Map(ovInst->GetLHS()),
            Map(ovInst->GetRHS())
        ));
      }
      case Inst::Kind::UNDEF: {
        auto *undefInst = static_cast<UndefInst *>(inst);
        return add(new UndefInst(undefInst->GetType()));
      }
      case Inst::Kind::PHI: {
        auto *phiInst = static_cast<PhiInst *>(inst);
        auto *phiNew = new PhiInst(phiInst->GetType());
        phis_.emplace_back(phiInst, phiNew);
        return add(phiNew);
      }
    }
  }

  /// Maps a value.
  Value *Map(Value *val)
  {
    switch (val->GetKind()) {
      case Value::Kind::INST: {
        return Map(static_cast<Inst *>(val));
      }
      case Value::Kind::GLOBAL: {
        switch (static_cast<Global *>(val)->GetKind()) {
          case Global::Kind::SYMBOL: return val;
          case Global::Kind::EXTERN: return val;
          case Global::Kind::FUNC: return val;
          case Global::Kind::BLOCK: return Map(static_cast<Block *>(val));
          case Global::Kind::ATOM: return val;
        }
      }
      case Value::Kind::EXPR: {
        return val;
      }
      case Value::Kind::CONST: {
        return val;
      }
    }
  }

  /// Maps a block.
  Block *Map(Block *block)
  {
    return blocks_[block];
  }

  /// Maps an instruction.
  Inst *Map(Inst *inst)
  {
    return insts_[inst];
  }


private:
  /// Call site being inlined.
  CallInst *call_;
  /// Entry block.
  Block *entry_;
  /// Called function.
  Func *callee_;
  /// Caller function.
  Func *caller_;
  /// Size of the caller's stack, to adjust callee offsets.
  unsigned stackSize_;
  /// Exit block.
  Block *exit_;
  /// Final PHI.
  PhiInst *phi_;
  /// Number of exit nodes.
  unsigned numExits_;
  /// Arguments.
  llvm::SmallVector<Inst *, 8> args_;
  /// Mapping from old to new instructions.
  llvm::DenseMap<Inst *, Inst *> insts_;
  /// Mapping from old to new blocks.
  llvm::DenseMap<Block *, Block *> blocks_;
  /// Block order.
  llvm::ReversePostOrderTraversal<Func *> rpot_;
  /// PHI instruction.
  llvm::SmallVector<std::pair<PhiInst *, PhiInst *>, 10> phis_;
};

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
        if (calleeFunc->IsVarArg()) {
          continue;
        }
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
          // TODO: inline tail calls
          continue;
        }

        InlineContext(static_cast<CallInst *>(&inst), calleeFunc).Inline();
        callee->eraseFromParent();
        calleeFunc->eraseFromParent();
        break;
      }
    }
  }
}

// -----------------------------------------------------------------------------
const char *InlinerPass::GetPassName() const
{
  return "Inliner";
}
