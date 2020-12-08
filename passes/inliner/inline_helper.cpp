// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include <sstream>

#include "passes/inliner/inline_helper.h"
#include "passes/inliner/trampoline_graph.h"



// -----------------------------------------------------------------------------
InlineHelper::InlineHelper(CallSite *call, Func *callee, TrampolineGraph &graph)
  : isTailCall_(call->Is(Inst::Kind::TAIL_CALL))
  , types_(call->type_begin(), call->type_end())
  , call_(call)
  , callAnnot_(call->GetAnnots())
  , entry_(call->getParent())
  , callee_(callee)
  , caller_(entry_->getParent())
  , exit_(nullptr)
  , numExits_(0)
  , rpot_(callee_)
  , graph_(graph)
{
  // Prepare the arguments.
  for (Ref<Inst> arg : call->args()) {
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
    case Inst::Kind::TAIL_CALL: {
      call_->eraseFromParent();
      call_ = nullptr;
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
      // Handle arguments separately.
      if (auto *argInst = ::cast_or_null<ArgInst>(&inst)) {
        insts_[argInst] = Duplicate(target, argInst);
      } else {
        // Duplicate the instruction, placing it at the desired point.
        if (auto *copy = Duplicate(target, &inst)) {
          assert(copy->GetNumRets() == inst.GetNumRets() && "invalid copy");
          for (unsigned i = 0, n = copy->GetNumRets(); i < n; ++i) {
            insts_[inst.GetSubValue(i)] = copy->GetSubValue(i);
          }
        }
      }
    }
  }

  // Apply PHI fixups.
  Fixup();

  // The call should have been erased at this point.
  assert(!call_ && "call not erased");
}

// -----------------------------------------------------------------------------
Inst *InlineHelper::Duplicate(Block *block, Inst *inst)
{
  auto phi = [&, this] (Block *block)
  {
    for (PhiInst &phi : exit_->phis()) {
      if (phi.HasValue(block)) {
        continue;
      }
      phi.Add(block, phi.GetValue(entry_));
    }

    if (call_) {
      call_->eraseFromParent();
      call_ = nullptr;
    }
  };

  auto ret = [&, this] (Block *block, llvm::ArrayRef<Ref<Inst>> insts)
  {
    if (!insts.empty()) {
      if (!phis_.empty()) {
        assert(phis_.size() == insts.size() && "invalid insts");
        for (unsigned i = 0, n = insts.size(); i < n; ++i) {
          phis_[i]->Add(block, insts[i]);
        }
      } else if (call_) {
        call_->replaceAllUsesWith<Inst>(insts);
      }
    }

    phi(block);
  };

  switch (inst->GetKind()) {
    // Convert tail calls to calls if caller does not tail.
    case Inst::Kind::TAIL_CALL: {
      if (isTailCall_) {
        block->AddInst(CloneVisitor::Clone(inst));
      } else {
        assert(exit_ && "missing block to return to");

        auto *callInst = static_cast<TailCallInst *>(inst);
        auto cloneCall = [&] (Block *cont) {
          return new CallInst(
              std::vector<Type>{ callInst->type_begin(), callInst->type_end() },
              Map(callInst->GetCallee()),
              CloneVisitor::Map(callInst->args()),
              cont,
              callInst->GetNumFixedArgs(),
              callInst->GetCallingConv(),
              Annot(inst)
          );
        };

        if (types_.empty()) {
          // Inlining a tail call into a void call site: discard all
          // returns and emit a call continuing on to the exit node.
          block->AddInst(cloneCall(exit_));
          phi(block);
        } else {
          const bool sameTypes = std::equal(
              types_.begin(), types_.end(),
              callInst->type_begin(), callInst->type_end()
          );

          if (sameTypes) {
            auto *inst = cloneCall(exit_);
            block->AddInst(inst);
            if (!phis_.empty()) {
              for (unsigned i = 0, n = phis_.size(); i < n; ++i) {
                phis_[i]->Add(block, inst->GetSubValue(i));
              }
            } else if (call_) {
              call_->replaceAllUsesWith(inst);
            }
            phi(block);
          } else {
            // If the types do not match or
            //
            //   call.T  $0, $f, ..., .Ltramp
            // .Ltramp:
            //   xext.T' $1, $0
            //   jmp  .Lexit
            // .Lexit:
            //   ...
            //
            Block *trampoline = new Block(exit_->getName());
            caller_->insertAfter(block->getIterator(), trampoline);
            auto *inst = cloneCall(trampoline);
            block->AddInst(inst);

            llvm::SmallVector<Ref<Inst>, 5> insts;
            for (unsigned i = 0, n = types_.size(); i < n; ++i) {
              const Type retTy = types_[i];
              if (i < inst->type_size()) {
                const Type callTy = inst->type(i);
                if (retTy == callTy) {
                  insts.push_back(inst->GetSubValue(i));
                } else {
                  auto *extInst = Convert(retTy, callTy, inst, {});
                  trampoline->AddInst(extInst);
                  insts.push_back(extInst);
                }
              } else {
                auto *undefInst = new UndefInst(retTy, {});
                trampoline->AddInst(undefInst);
                insts.push_back(undefInst);
              }
            }

            ret(trampoline, insts);
            trampoline->AddInst(new JumpInst(exit_, {}));
          }
        }
      }
      return nullptr;
    }
    // Propagate value if caller does not tail.
    case Inst::Kind::RETURN: {
      if (isTailCall_) {
        block->AddInst(CloneVisitor::Clone(inst));
      } else {
        auto *retInst = static_cast<ReturnInst *>(inst);

        llvm::SmallVector<Ref<Inst>, 5> insts;
        for (unsigned i = 0, n = types_.size(); i < n; ++i) {
          if (i < retInst->arg_size()) {
            Ref<Inst> newVal = Map(retInst->arg(i));
            auto retType = newVal.GetType();
            if (types_[i] != retType) {
              auto *extInst = Convert(types_[i], retType, newVal, {});
              block->AddInst(extInst);
              insts.push_back(extInst);
            } else {
              insts.push_back(newVal);
            }
          } else {
            auto *undefInst = new UndefInst(types_[i], {});
            block->AddInst(undefInst);
            insts.push_back(undefInst);
          }
        }

        ret(block, insts);
        block->AddInst(new JumpInst(exit_, {}));
      }
      return nullptr;
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
      if (auto reg = ::cast_or_null<ConstantReg>(mov->GetArg())) {
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
                new ConstantReg(ConstantReg::Kind::SP),
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
    case Inst::Kind::JUMP:
    case Inst::Kind::JUMP_COND:
    case Inst::Kind::RAISE:
    case Inst::Kind::TRAP: {
      auto *newTerm = CloneVisitor::Clone(inst);
      block->AddInst(newTerm);
      return newTerm;
    }
    case Inst::Kind::ARG: {
      llvm_unreachable("arguments are inlined separately");
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
Ref<Inst> InlineHelper::Duplicate(Block *block, ArgInst *arg) {
  // Arguments can map to a reference instead of a full instruction.
  auto argType = arg->GetType(0);
  if (arg->GetIdx() < args_.size()) {
    Ref<Inst> valInst = args_[arg->GetIdx()];
    auto valType = valInst.GetType();
    if (argType == valType) {
      return valInst;
    }
    auto *extInst = Convert(argType, valType, valInst, Annot(arg));
    block->AddInst(extInst);
    return extInst;
  } else {
    auto *undefInst = new UndefInst(argType, Annot(arg));
    block->AddInst(undefInst);
    return undefInst;
  }
}

// -----------------------------------------------------------------------------
AnnotSet InlineHelper::Annot(const Inst *inst)
{
  switch (inst->GetKind()) {
    case Inst::Kind::CALL:
    case Inst::Kind::TAIL_CALL:
    case Inst::Kind::INVOKE: {
      ConstRef<Inst> callee = static_cast<const CallSite *>(inst)->GetCallee();
      AnnotSet annots = inst->GetAnnots();
      if (graph_.NeedsTrampoline(callee)) {
        if (auto *frame = callAnnot_.Get<CamlFrame>()) {
          annots.Set<CamlFrame>(*frame);
        }
      }
      return annots;
    }
    default: {
      return inst->GetAnnots();
    }
  }
}

// -----------------------------------------------------------------------------
Inst *InlineHelper::Convert(
    Type argType,
    Type valType,
    Ref<Inst> valInst,
    AnnotSet &&annot)
{
  if (IsIntegerType(argType) && IsIntegerType(valType)) {
    auto argSize = GetSize(argType);
    auto valSize = GetSize(valType);
    if (argSize < valSize) {
      // Zero-extend integral argument to match.
      return new TruncInst(argType, valInst, std::move(annot));
    } else if (argSize > valSize) {
      // Truncate integral argument to match.
      return new XExtInst(argType, valInst, std::move(annot));
    } else {
      // Bitcast or value-pointer conversion.
      return new MovInst(argType, valInst, std::move(annot));
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

    // Form a name, containing the callee name, along with
    // the caller name to make it unique.
    std::ostringstream os;
    os << block->GetName();
    os << "$" << caller_->GetName();
    os << "$" << callee_->GetName();
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
      if (auto *phi = ::cast_or_null<PhiInst>(use.getUser())) {
        use = newEntry;
      }
    }
    entry_ = newEntry;
  }

  // Count the number of blocks which jump to return from the inlined function.
  unsigned numBlocks = 0;
  for (auto *block : rpot_) {
    numBlocks++;
    switch (block->GetTerminator()->GetKind()) {
      case Inst::Kind::CALL:
      case Inst::Kind::INVOKE:
      case Inst::Kind::JUMP_COND:
      case Inst::Kind::JUMP:
      case Inst::Kind::SWITCH: {
        // Control flow inside the function.
        break;
      }
      case Inst::Kind::TAIL_CALL:
      case Inst::Kind::RETURN: {
        // Exit back to callee.
        numExits_++;
        break;
      }
      case Inst::Kind::TRAP:
      case Inst::Kind::RAISE: {
        // Never return.
        break;
      }
      default: {
        llvm_unreachable("not a terminator");
      }
    }
  }

  if (numExits_ == 0) {
    // The called function never returns - remove from PHIs and replace
    // the used values with undefined added before the call, guaranteed to
    // dominate all potential uses.
    std::vector<Ref<Inst>> undefs;
    for (unsigned i = 0, n = call_->GetNumRets(); i < n; ++i) {
      Inst *undef = new UndefInst(call_->GetType(i), {});
      entry_->AddInst(undef, call_);
      undefs.push_back(undef);
    }
    call_->replaceAllUsesWith(undefs);

    // If the call had a successor, remove all incoming edges from the call.
    switch (call_->GetKind()) {
      case Inst::Kind::CALL: {
        auto *call = static_cast<CallInst *>(call_);
        const Block *parent = call_->getParent();
        for (PhiInst &phi : call->GetCont()->phis()) {
          if (phi.HasValue(parent)) {
            phi.Remove(parent);
          }
        }
        break;
      }
      case Inst::Kind::INVOKE: {
        llvm_unreachable("not implemented");
      }
      case Inst::Kind::TAIL_CALL: {
        break;
      }
      default: {
        llvm_unreachable("not a call");
      }
    }

    // Erase the call.
    call_->eraseFromParent();
    call_ = nullptr;
  } else {
    // If the call success has other incoming edges, place the phis
    // into a fresh block preceding it and wire the phis into any
    // prior instructions.
    if (exit_->pred_size() != 1) {
      Block *newExit = new Block((exit_->getName() + "exit").str());
      caller_->AddBlock(newExit, exit_);
      JumpInst *newJump = new JumpInst(exit_, {});
      newExit->AddInst(newJump);

      Block *parent = call_->getParent();
      for (PhiInst &phi : exit_->phis()) {
        Ref<Inst> incoming = phi.GetValue(parent);
        phi.Remove(parent);
        phi.Add(newExit, incoming);
      }
      exit_ = newExit;
    }

    if (numExits_ > 1) {
      // Create a PHI node if there are multiple exits.
      if (types_.empty()) {
        assert(call_->use_empty() && "void call has uses");
      } else {
        // For each index, create a phi and replace the
        // corresponding return value of the original call.
        for (unsigned i = 0, n = types_.size(); i < n; ++i) {
          const Type ty = types_[i];
          PhiInst *phi = new PhiInst(types_[i]);
          exit_->AddPhi(phi);
          phis_.push_back(phi);
        }
        call_->replaceAllUsesWith<PhiInst>(phis_);
      }
      call_->eraseFromParent();
      call_ = nullptr;
    }
  }
  assert((!call_ || numExits_ == 1) && "call not erased");
}
