// This file if part of the genm-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#pragma once

#include <unordered_map>

#include "core/insts.h"
#include "core/insts_binary.h"
#include "core/insts_call.h"
#include "core/insts_control.h"
#include "core/insts_memory.h"
#include "core/insts_unary.h"

class Inst;
class Block;
class Global;



/**
 * Helper class which clones instructions.
 */
class InstClone {
public:
  /// Cleanup.
  virtual ~InstClone();

  /// Maps a block to a new one.
  Block *Clone(Block *block);
  /// Maps an instruction to a new one.
  Inst *Clone(Inst *inst);
  /// Maps a value to a potentially new one.
  Value *Clone(Value *value);

  /// Applies PHI fixups.
  void Fixup();

protected:
  /// Actually clones a block.
  virtual Block *Make(Block *block) = 0;

  /// Actually clones an instruction.
  virtual Inst *Make(Inst *inst);

  // Control flow.
  virtual Inst *Make(CallInst *i);
  virtual Inst *Make(TailCallInst *i);
  virtual Inst *Make(InvokeInst *i);
  virtual Inst *Make(TailInvokeInst *i);
  virtual Inst *Make(ReturnInst *i);
  virtual Inst *Make(JumpCondInst *i);
  virtual Inst *Make(JumpIndirectInst *i);
  virtual Inst *Make(JumpInst *i);
  virtual Inst *Make(SwitchInst *i);
  virtual Inst *Make(TrapInst *i);
  // Memory.
  virtual Inst *Make(LoadInst *i);
  virtual Inst *Make(StoreInst *i);
  virtual Inst *Make(ExchangeInst *i);
  virtual Inst *Make(SetInst *i);
  virtual Inst *Make(VAStartInst *i);
  virtual Inst *Make(FrameInst *i);
  // Ternary.
  virtual Inst *Make(SelectInst *i);
  // Unary.
  virtual Inst *Make(AbsInst *i)      { return MakeUnary<AbsInst>(i); }
  virtual Inst *Make(NegInst *i)      { return MakeUnary<NegInst>(i); }
  virtual Inst *Make(SqrtInst *i)     { return MakeUnary<SqrtInst>(i); }
  virtual Inst *Make(SinInst *i)      { return MakeUnary<SinInst>(i); }
  virtual Inst *Make(CosInst *i)      { return MakeUnary<CosInst>(i); }
  virtual Inst *Make(SExtInst *i)     { return MakeUnary<SExtInst>(i); }
  virtual Inst *Make(ZExtInst *i)     { return MakeUnary<ZExtInst>(i); }
  virtual Inst *Make(FExtInst *i)     { return MakeUnary<FExtInst>(i); }
  virtual Inst *Make(TruncInst *i)    { return MakeUnary<TruncInst>(i); }
  // Binary instructions.
  virtual Inst *Make(CmpInst *i);
  virtual Inst *Make(DivInst *i)      { return MakeBinary<DivInst>(i); }
  virtual Inst *Make(RemInst *i)      { return MakeBinary<RemInst>(i); }
  virtual Inst *Make(MulInst *i)      { return MakeBinary<MulInst>(i); }
  virtual Inst *Make(AddInst *i)      { return MakeBinary<AddInst>(i); }
  virtual Inst *Make(SubInst *i)      { return MakeBinary<SubInst>(i); }
  virtual Inst *Make(AndInst *i)      { return MakeBinary<AndInst>(i); }
  virtual Inst *Make(OrInst *i)       { return MakeBinary<OrInst>(i); }
  virtual Inst *Make(SllInst *i)      { return MakeBinary<SllInst>(i); }
  virtual Inst *Make(SraInst *i)      { return MakeBinary<SraInst>(i); }
  virtual Inst *Make(SrlInst *i)      { return MakeBinary<SrlInst>(i); }
  virtual Inst *Make(XorInst *i)      { return MakeBinary<XorInst>(i); }
  virtual Inst *Make(RotlInst *i)     { return MakeBinary<RotlInst>(i); }
  virtual Inst *Make(PowInst *i)      { return MakeBinary<PowInst>(i); }
  virtual Inst *Make(CopySignInst *i) { return MakeBinary<CopySignInst>(i); }
  // Overflow.
  virtual Inst *Make(AddUOInst *i)    { return MakeOverflow<AddUOInst>(i); }
  virtual Inst *Make(MulUOInst *i)    { return MakeOverflow<MulUOInst>(i); }
  // Special.
  virtual Inst *Make(MovInst *i);
  virtual Inst *Make(UndefInst *i);
  virtual Inst *Make(PhiInst *i);
  virtual Inst *Make(ArgInst *i);

private:
  /// Clones a binary instruction.
  template<typename T> Inst *MakeBinary(BinaryInst *inst);
  /// Clones a unary instruction.
  template<typename T> Inst *MakeUnary(UnaryInst *inst);
  /// Clones an overflow instruction.
  template<typename T> Inst *MakeOverflow(OverflowInst *inst);
  /// Clones an argument list.
  template<typename T> std::vector<Inst *> MakeArgs(T *inst);

private:
  /// Map of cloned blocks.
  std::unordered_map<Block *, Block *> blocks_;
  /// Map of cloned instructions.
  std::unordered_map<Inst *, Inst *> insts_;
};
