// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include <sstream>

#include "passes/inliner/inline_helper.h"
#include "passes/inliner/trampoline_graph.h"


// -----------------------------------------------------------------------------
InlineHelper::InlineHelper(CallSite *call, Func *callee, TrampolineGraph &graph)
  : isTailCall_(call->Is(Inst::Kind::TCALL))
  , type_(call->GetType())
  , call_(isTailCall_ ? nullptr : call)
  , callCallee_(call->GetCallee())
  , callAnnot_(call->GetAnnots())
  , entry_(call->getParent())
  , callee_(callee)
  , caller_(entry_->getParent())
  , exit_(nullptr)
  , phi_(nullptr)
  , numExits_(0)
  , rpot_(callee_)
  , graph_(graph)
{
  // Prepare the arguments.
  for (auto *arg : call->args()) {
    args_.push_back(arg);
  }

  // Adjust the caller's stack.
  {
    auto *caller = entry_->getParent();
    unsigned maxIndex = 0;
    for (auto &object : caller->objects()) {
      maxIndex = std::max(maxIndex, object.Index);
    }
    for (auto &object : callee->objects()) {
      unsigned newIndex = maxIndex + object.Index + 1;
      frameIndices_.insert({ object.Index, newIndex });
      caller->AddStackObject(newIndex, object.Size, object.Alignment);
    }
  }

  // Split the entry if a label to it is needed.
  switch (call->GetKind()) {
    case Inst::Kind::CALL: {
      exit_ = static_cast<CallInst *>(call)->GetCont();
      SplitEntry();
      break;
    }
    case Inst::Kind::INVOKE: {
      exit_ = static_cast<InvokeInst *>(call)->GetCont();
      SplitEntry();
      break;
    }
    case Inst::Kind::TCALL: {
      call->eraseFromParent();
      break;
    }
    default: {
      llvm_unreachable("invalid call site");
    }
  }

  // Find an equivalent for all blocks in the target function.
  DuplicateBlocks();
}

// -----------------------------------------------------------------------------
void InlineHelper::Inline()
{
  // Inline all blocks from the callee.
  for (auto *block : rpot_) {
    // Decide which block to place the instruction in.
    auto *target = Map(block);
    for (auto &inst : *block) {
      // Duplicate the instruction, placing it at the desired point.
      insts_[&inst] = Duplicate(target, &inst);
    }
  }

  // Apply PHI fixups.
  Fixup();

  // Remove the call site (can stay there if the function never returns).
  if (call_) {
    if (call_->GetNumRets() > 0) {
      Inst *undef = new UndefInst(call_->GetType(0), call_->GetAnnots());
      entry_->AddInst(undef, call_);
      call_->replaceAllUsesWith(undef);
    }
    call_->eraseFromParent();
  }
}

// -----------------------------------------------------------------------------
Inst *InlineHelper::Duplicate(Block *block, Inst *inst)
{
  auto ret = [this] (Block *block, Inst *value) {
    if (value) {
      if (phi_) {
        phi_->Add(block, value);
      } else if (call_) {
        if (call_->HasAnnot<CamlValue>() && !value->HasAnnot<CamlValue>()) {
          value->SetAnnot<CamlValue>();
        }
        call_->replaceAllUsesWith(value);
      }
    }
    if (call_) {
      call_->eraseFromParent();
      call_ = nullptr;
    }
  };

  switch (inst->GetKind()) {
    // Convert tail calls to calls if caller does not tail.
    case Inst::Kind::TCALL: {
      if (isTailCall_) {
        block->AddInst(CloneVisitor::Clone(inst));
      } else {
        assert(exit_ && "missing block to return to");

        auto *callInst = static_cast<TailCallInst *>(inst);
        auto cloneCall = [&] (Block *cont) {
          return new CallInst(
              callInst->GetType(),
              Map(callInst->GetCallee()),
              CloneVisitor::CloneArgs<TailCallInst>(callInst),
              cont,
              callInst->GetNumFixedArgs(),
              callInst->GetCallingConv(),
              Annot(inst)
          );
        };

        if (type_) {
          if (auto type = callInst->GetType()) {
            // If the types do not match or
            //
            //   call.T  $0, $f, ..., .Ltramp
            // .Ltramp:
            //   xext.T' $1, $0
            //   jmp  .Lexit
            // .Lexit:
            //   ...
            //
            if (type_ == type) {
              auto *callValue = cloneCall(exit_);
              block->AddInst(callValue);
              ret(block, callValue);
            } else {
              Block *trampoline = new Block(exit_->getName());
              caller_->insertAfter(block->getIterator(), trampoline);
              auto *callValue = cloneCall(trampoline);
              block->AddInst(callValue);
              auto *extInst = XExtOrTrunc(*type_, *type, callValue, {});
              trampoline->AddInst(extInst);
              ret(trampoline, extInst);
              trampoline->AddInst(new JumpInst(exit_, {}));
            }
          } else {
            Block *trampoline = new Block(exit_->getName());
            caller_->insertAfter(block->getIterator(), trampoline);
            block->AddInst(cloneCall(trampoline));
            auto *undefInst = new UndefInst(*type_, {});
            trampoline->AddInst(undefInst);
            ret(trampoline, undefInst);
            trampoline->AddInst(new JumpInst(exit_, {}));
          }
        } else {
          // Inlining a tail call which does not yield a value into
          // a void call site: add the call, continuing at exit.
          block->AddInst(cloneCall(exit_));
        }
      }
      return nullptr;
    }
    // Propagate value if caller does not tail.
    case Inst::Kind::RET: {
      if (isTailCall_) {
        block->AddInst(CloneVisitor::Clone(inst));
      } else {
        if (type_) {
          auto *retInst = static_cast<ReturnInst *>(inst);
          if (auto *oldVal = retInst->GetValue()) {
            auto *newVal = Map(oldVal);
            auto retType = newVal->GetType(0);
            if (type_ != retType) {
              auto *extInst = XExtOrTrunc(*type_, retType, newVal, {});
              block->AddInst(extInst);
              ret(block, extInst);
            } else {
              ret(block, newVal);
            }
          } else {
            auto *undefInst = new UndefInst(*type_, {});
            block->AddInst(undefInst);
            ret(block, new UndefInst(*type_, {}));
          }
        } else {
          ret(block, nullptr);
        }
        block->AddInst(new JumpInst(exit_, {}));
      }
      return nullptr;
    }
    // Map argument to incoming value.
    case Inst::Kind::ARG: {
      auto *argInst = static_cast<ArgInst *>(inst);
      auto argType = argInst->GetType(0);
      if (argInst->GetIdx() < args_.size()) {
        auto *valInst = args_[argInst->GetIdx()];
        auto valType = valInst->GetType(0);
        if (argType == valType) {
          return valInst;
        }
        auto *extInst = XExtOrTrunc(argType, valType, valInst, Annot(argInst));
        block->AddInst(extInst);
        return extInst;
      } else {
        auto *undefInst = new UndefInst(argType, Annot(argInst));
        block->AddInst(undefInst);
        return undefInst;
      }
    }
    // Adjust stack offset.
    case Inst::Kind::FRAME: {
      auto *frameInst = static_cast<FrameInst *>(inst);
      auto it = frameIndices_.find(frameInst->GetObject());
      assert(it != frameIndices_.end() && "frame index out of range");
      auto *newFrameInst = new FrameInst(
          frameInst->GetType(),
          new ConstantInt(it->second),
          new ConstantInt(frameInst->GetOffset()),
          Annot(frameInst)
      );
      block->AddInst(newFrameInst);
      return newFrameInst;
    }
    // The semantics of mov change.
    case Inst::Kind::MOV: {
      auto *mov = static_cast<MovInst *>(inst);
      if (auto *reg = ::dyn_cast_or_null<ConstantReg>(mov->GetArg())) {
        Inst *newMov;
        switch (reg->GetValue()) {
          // Instruction which take the return address of a function.
          case ConstantReg::Kind::RET_ADDR: {
            // If the callee is annotated, add a frame to the jump.
            if (exit_) {
              newMov = new MovInst(mov->GetType(), exit_, mov->GetAnnots());
            } else {
              newMov = CloneVisitor::Clone(mov);
            }
            break;
          }
          // Instruction which takes the frame address: take SP instead.
          case ConstantReg::Kind::FRAME_ADDR: {
            newMov = new MovInst(
                mov->GetType(),
                new ConstantReg(ConstantReg::Kind::RSP),
                mov->GetAnnots()
            );
            break;
          }
          default: {
            newMov = CloneVisitor::Clone(mov);
            break;
          }
        }
        block->AddInst(newMov);
        return newMov;
      } else {
        auto *newMov = CloneVisitor::Clone(mov);
        block->AddInst(newMov);
        return newMov;
      }
    }
    // Terminators need to remove all other instructions in target block.
    case Inst::Kind::INVOKE:
    case Inst::Kind::JMP:
    case Inst::Kind::JCC:
    case Inst::Kind::RAISE:
    case Inst::Kind::TRAP: {
      auto *newTerm = CloneVisitor::Clone(inst);
      block->AddInst(newTerm);
      return newTerm;
    }
    // Simple instructions which can be cloned.
    default: {
      auto *newInst = CloneVisitor::Clone(inst);
      block->AddInst(newInst);
      return newInst;
    }
  }
}

// -----------------------------------------------------------------------------
AnnotSet InlineHelper::Annot(const Inst *inst)
{
  AnnotSet annots = inst->GetAnnots();

  const Value *callee;
  switch (inst->GetKind()) {
    case Inst::Kind::CALL:
    case Inst::Kind::TCALL:
    case Inst::Kind::INVOKE: {
      callee = static_cast<const CallSite *>(inst)->GetCallee();
      break;
    }
    default: {
      return annots;
    }
  }

  if (graph_.NeedsTrampoline(callee)) {
    if (callAnnot_.Has<CamlFrame>()) {
      annots.Set<CamlFrame>();
    }
  }
  return annots;
}

// -----------------------------------------------------------------------------
Inst *InlineHelper::XExtOrTrunc(
    Type argType,
    Type valType,
    Inst *valInst,
    AnnotSet &&annot)
{
  if (IsIntegerType(argType) && IsIntegerType(valType)) {
    if (GetSize(argType) < GetSize(valType)) {
      // Zero-extend integral argument to match.
      return new TruncInst(argType, valInst, std::move(annot));
    } else {
      // Truncate integral argument to match.
      return new XExtInst(argType, valInst, std::move(annot));
    }
  }
  llvm_unreachable("Cannot extend/cast type");
}

// -----------------------------------------------------------------------------
void InlineHelper::DuplicateBlocks()
{
  auto *after = entry_;
  for (Block *block : rpot_) {
    if (block == &callee_->getEntryBlock()) {
      blocks_[block] = entry_;
      continue;
    }

    // Duplicate the block and add it to the callee now.
    auto *newBlock = new Block(block->getName());
    caller_->insertAfter(after->getIterator(), newBlock);
    after = newBlock;
    blocks_[block] = newBlock;
  }
}

// -----------------------------------------------------------------------------
void InlineHelper::SplitEntry()
{
  // If entry address is taken in callee, split entry.
  if (!callee_->getEntryBlock().use_empty()) {
    auto *newEntry = entry_->splitBlock(call_->getIterator());
    entry_->AddInst(new JumpInst(newEntry, {}));
    for (auto it = entry_->use_begin(); it != entry_->use_end(); ) {
      Use &use = *it++;
      if (auto *phi = ::dyn_cast_or_null<PhiInst>(use.getUser())) {
        use = newEntry;
      }
    }
    entry_ = newEntry;
  }

  // Checks if the function has a single exit.
  unsigned numBlocks = 0;
  for (auto *block : rpot_) {
    numBlocks++;
    if (block->succ_empty()) {
      numExits_++;
    }
  }

  // Create a PHI node if there are multiple exits.
  if (numExits_ > 1) {
    if (type_) {
      phi_ = new PhiInst(*type_);
      if (call_->HasAnnot<CamlValue>()) {
        phi_->SetAnnot<CamlValue>();
      }
      exit_->AddPhi(phi_);
      call_->replaceAllUsesWith(phi_);
    }
    call_->eraseFromParent();
    call_ = nullptr;
  }
}
