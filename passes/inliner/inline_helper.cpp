// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include <sstream>

#include "passes/inliner/inline_helper.h"
#include "passes/inliner/trampoline_graph.h"


// -----------------------------------------------------------------------------
void InlineHelper::Inline()
{
  // Inline all blocks from the callee.
  for (auto *block : rpot_) {
    // Decide which block to place the instruction in.
    auto *target = Map(block);
    Inst *insert;
    if (target->empty()) {
      insert = nullptr;
    } else {
      if (target == entry_) {
        insert = exit_ ? nullptr : call_;
      } else {
        insert = &*target->begin();
      }
    }

    for (auto &inst : *block) {
      // Duplicate the instruction, placing it at the desired point.
      insts_[&inst] = Duplicate(target, insert, &inst);
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
Inst *InlineHelper::Duplicate(Block *block, Inst *&before, Inst *inst)
{
  auto add = [block, before] (Inst *inst) {
    block->AddInst(inst, before);
    return inst;
  };

  auto replace = [block, this] (Inst *value) {
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

  auto ret = [&replace, block, this] (Inst *value) {
    replace(value);
    if (numExits_ > 1 || needsExit_) {
      block->AddInst(new JumpInst(exit_, {}));
    }
  };

  switch (inst->GetKind()) {
    // Convert tail calls to calls if caller does not tail.
    case Inst::Kind::TCALL: {
      if (isTailCall_) {
        add(CloneVisitor::Clone(inst));
      } else {
        auto *callInst = static_cast<TailCallInst *>(inst);
        Inst *callValue = add(new CallInst(
            callInst->GetType(),
            Map(callInst->GetCallee()),
            CloneVisitor::CloneArgs<TailCallInst>(callInst),
            callInst->GetNumFixedArgs(),
            callInst->GetCallingConv(),
            Annot(inst)
        ));

        if (type_) {
          if (auto type = callInst->GetType()) {
            if (type_ == type) {
              ret(callValue);
            } else {
              ret(add(Extend(*type_, *type, callValue, {})));
            }
          } else {
            ret(add(new UndefInst(*type_, {})));
          }
        } else {
          ret(nullptr);
        }
      }
      return nullptr;
    }
    // Convert tail invokes to invokes if caller does not tail.
    case Inst::Kind::TINVOKE: {
      if (isTailCall_) {
        add(CloneVisitor::Clone(inst));
      } else {
        llvm_unreachable("not implemented");
      }
      return nullptr;
    }
    // Propagate value if caller does not tail.
    case Inst::Kind::RET: {
      if (isTailCall_) {
        add(CloneVisitor::Clone(inst));
      } else {
        if (type_) {
          if (auto *val = static_cast<ReturnInst *>(inst)->GetValue()) {
            auto *retInst = Map(val);
            auto retType = retInst->GetType(0);
            if (type_ != retType) {
              ret(add(Extend(*type_, retType, retInst, {})));
            } else {
              ret(retInst);
            }
          } else {
            ret(add(new UndefInst(*type_, {})));
          }
        } else {
          ret(nullptr);
        }
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
        if (argType != valType) {
          return add(Extend(argType, valType, valInst, Annot(argInst)));
        }
        return valInst;
      } else {
        return add(new UndefInst(argType, Annot(argInst)));
      }
    }
    // Adjust stack offset.
    case Inst::Kind::FRAME: {
      auto *frameInst = static_cast<FrameInst *>(inst);
      auto it = frameIndices_.find(frameInst->GetObject());
      assert(it != frameIndices_.end() && "frame index out of range");
      return add(new FrameInst(
          frameInst->GetType(),
          new ConstantInt(it->second),
          new ConstantInt(frameInst->GetOffset()),
          Annot(frameInst)
      ));
    }
    // The semantics of mov change.
    case Inst::Kind::MOV: {
      auto *mov = static_cast<MovInst *>(inst);
      if (auto *reg = ::dyn_cast_or_null<ConstantReg>(mov->GetArg())) {
        switch (reg->GetValue()) {
          // Instruction which take the return address of a function.
          case ConstantReg::Kind::RET_ADDR: {
            // If the callee is annotated, add a frame to the jump.
            if (exit_) {
              return add(new MovInst(mov->GetType(), exit_, mov->GetAnnots()));
            } else {
              return add(CloneVisitor::Clone(mov));
            }
          }
          // Instruction which takes the frame address: take SP instead.
          case ConstantReg::Kind::FRAME_ADDR: {
            return add(new MovInst(
                mov->GetType(),
                new ConstantReg(ConstantReg::Kind::RSP),
                mov->GetAnnots()
            ));
          }
          default: {
            return add(CloneVisitor::Clone(mov));
          }
        }
      } else {
        return add(CloneVisitor::Clone(mov));
      }
    }
    // Terminators need to remove all other instructions in target block.
    case Inst::Kind::INVOKE:
    case Inst::Kind::JMP:
    case Inst::Kind::JCC:
    case Inst::Kind::RAISE:
    case Inst::Kind::TRAP: {
      auto *term = add(CloneVisitor::Clone(inst));
      if (before) {
        for (auto it = before->getIterator(); it != block->end(); ) {
          auto *inst = &*it++;
          call_ = call_ == inst ? nullptr : call_;
          phi_ = phi_ == inst ? nullptr : phi_;
          inst->eraseFromParent();
        }

        for (auto it = block->user_begin(); it != block->user_end(); ) {
          User *user = *it++;
          if (auto *phi = ::dyn_cast_or_null<PhiInst>(user)) {
            phi->Remove(block);
          }
        }
      }
      return term;
    }
    // Simple instructions which can be cloned.
    default: {
      return add(CloneVisitor::Clone(inst));
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
      callee = static_cast<const CallSite<Inst> *>(inst)->GetCallee();
      break;
    case Inst::Kind::TCALL:
    case Inst::Kind::INVOKE:
    case Inst::Kind::TINVOKE:
      callee = static_cast<const CallSite<TerminatorInst> *>(inst)->GetCallee();
      break;
    default:
      return annots;
  }

  if (graph_.NeedsTrampoline(callee)) {
    if (callAnnot_.Has<CamlFrame>()) {
      annots.Set<CamlFrame>();
    }
  }
  return annots;
}

// -----------------------------------------------------------------------------
Inst *InlineHelper::Extend(
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
      return new ZExtInst(argType, valInst, std::move(annot));
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
    const bool canUseEntry = numExits_ <= 1 && !needsExit_;
    if (block->succ_empty() && !isTailCall_ && canUseEntry) {
      blocks_[block] = exit_;
      continue;
    }


    static int uniqueID = 0;
    // Form a name, containing the callee name, along with
    // the caller name to make it unique.
    std::ostringstream os;
    os << block->GetName();
    os << "$inline";
    os << "$" << caller_->GetName();
    os << "$" << callee_->GetName();
    os << "$" << uniqueID++;
    auto *newBlock = new Block(os.str());
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

  // Split the block if the callee's CFG has multiple blocks.
  if (numBlocks > 1 || needsExit_) {
    exit_ = entry_->splitBlock(++call_->getIterator());
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
