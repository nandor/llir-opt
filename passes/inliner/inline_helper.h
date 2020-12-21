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
  /**
   * Initialises the inliner.
   *
   * @param call    Call site to inline into
   * @param callee  Callee to inline into the call site.
   * @param graph   OCaml trampoline graph.
   */
  InlineHelper(CallSite *call, Func *callee, TrampolineGraph &graph);

  /// Inlines the function.
  void Inline();

private:
  /// Creates a copy of an instruction.
  Inst *Duplicate(Block *block, Inst *inst);
  /// Creates a copy of an argument.
  Ref<Inst> Duplicate(Block *block, ArgInst *arg);

  /// Maps a block.
  Block *Map(Block *block) override { return blocks_[block]; }
  /// Maps an instruction.
  Ref<Inst> Map(Ref<Inst> inst) override { return insts_[inst]; }

  /// Inlines annotations.
  AnnotSet Annot(const Inst *inst) override;

  /// Extends a value from one type to another.
  Inst *Convert(
      Type argType,
      Type valType,
      Ref<Inst> valInst,
      AnnotSet &&annot
  );

  /// Duplicates blocks from the source function.
  void DuplicateBlocks();
  /// Split the entry.
  void SplitEntry();

private:
  /// Flag indicating if the call is a tail call.
  const bool isTailCall_;
  /// Return type of the call.
  const std::vector<Type> types_;
  /// Call site being inlined.
  CallSite *call_;
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
  /// Catch block.
  Block *throw_;
  /// Split-off part of the throw block to accommodate raise-turned-jump.
  Block *throwSplit_;
  /// PHIs for raise values.
  std::vector<Ref<PhiInst>> raisePhis_;
  /// PHIs from the landing pad.
  std::vector<Ref<PhiInst>> landPhis_;
  /// Final PHI.
  std::vector<Ref<PhiInst>> phis_;
  /// Number of exit nodes.
  unsigned numExits_;
  /// Arguments.
  llvm::SmallVector<Ref<Inst>, 8> args_;
  /// Mapping from old to new blocks.
  llvm::DenseMap<Block *, Block *> blocks_;
  /// Map of cloned instructions.
  std::unordered_map<Ref<Inst>, Ref<Inst>> insts_;
  /// Block order.
  llvm::ReversePostOrderTraversal<Func *> rpot_;
  /// Graph which determines calls needing trampolines.
  TrampolineGraph &graph_;
};

