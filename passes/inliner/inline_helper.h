// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#pragma once

#include <llvm/ADT/PostOrderIterator.h>

#include "core/block.h"
#include "core/cast.h"
#include "core/clone.h"
#include "core/func.h"
#include "core/cfg.h"

class TrampolineGraph;



/**
 * Inline clone helper
 */
class InlineHelper final : public CloneVisitor {
public:
  template<typename T>
  InlineHelper(T *call, Func *callee, TrampolineGraph &graph)
    : isTailCall_(call->Is(Inst::Kind::TCALL) || call->Is(Inst::Kind::TINVOKE))
    , isVirtCall_(call->Is(Inst::Kind::INVOKE) || call->Is(Inst::Kind::TINVOKE))
    , type_(call->GetType())
    , call_(isTailCall_ ? nullptr : call)
    , callAnnot_(call->GetAnnots())
    , entry_(call->getParent())
    , callee_(callee)
    , caller_(entry_->getParent())
    , exit_(nullptr)
    , phi_(nullptr)
    , numExits_(0)
    , needsExit_(false)
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

    // Exit is needed when C is inlined into OCaml.
    for (Block *block : rpot_) {
      for (Inst &inst : *block) {
        if (auto *mov = ::dyn_cast_or_null<MovInst>(&inst)) {
          if (auto *reg = ::dyn_cast_or_null<ConstantReg>(mov->GetArg())) {
            if (reg->GetValue() == ConstantReg::Kind::RET_ADDR) {
              needsExit_ = true;
            }
          }
        }
      }
    }

    if (isTailCall_) {
      call->eraseFromParent();
    } else {
      SplitEntry();
    }

    // Find an equivalent for all blocks in the target function.
    DuplicateBlocks();
  }

  /// Inlines the function.
  void Inline();

private:
  /// Creates a copy of an instruction.
  Inst *Duplicate(Block *block, Inst *&before, Inst *inst);

  /// Maps a block.
  Block *Map(Block *block) override { return blocks_[block]; }
  /// Maps an instruction.
  Inst *Map(Inst *inst) override { return insts_[inst]; }

  /// Inlines annotations.
  AnnotSet Annot(const Inst *inst) override;

  /// Extends a value from one type to another.
  Inst *Extend(Type argType, Type valType, Inst *valInst, AnnotSet &&annot);

  /// Duplicates blocks from the source function.
  void DuplicateBlocks();
  /// Split the entry.
  void SplitEntry();

private:
  /// Flag indicating if the call is a tail call.
  const bool isTailCall_;
  /// Flag indicating if the call is a virtual call.
  const bool isVirtCall_;
  /// Return type of the call.
  const std::optional<Type> type_;
  /// Call site being inlined.
  Inst *call_;
  /// Annotations of the original call.
  const AnnotSet callAnnot_;
  /// Entry block.
  Block *entry_;
  /// Called function.
  Func *callee_;
  /// Caller function.
  Func *caller_;
  /// Mapping from callee to caller frame indices.
  llvm::DenseMap<unsigned, unsigned> frameIndices_;
  /// Exit block.
  Block *exit_;
  /// Final PHI.
  PhiInst *phi_;
  /// Number of exit nodes.
  unsigned numExits_;
  /// Flag to indicate if a separate exit label is needed.
  bool needsExit_;
  /// Arguments.
  llvm::SmallVector<Inst *, 8> args_;
  /// Mapping from old to new blocks.
  llvm::DenseMap<Block *, Block *> blocks_;
  /// Map of cloned instructions.
  std::unordered_map<Inst *, Inst *> insts_;
  /// Block order.
  llvm::ReversePostOrderTraversal<Func *> rpot_;
  /// Graph which determines calls needing trampolines.
  TrampolineGraph &graph_;
};

