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
class CloneVisitor {
public:
  /// Destroys the visitor.
  virtual ~CloneVisitor();

  /// Maps a block to a new one.
  virtual Block *Map(Block *block) = 0;
  /// Maps an instruction to a new one.
  virtual Inst *Map(Inst *inst) = 0;
  /// Maps a value to a potentially new one.
  virtual Value *Map(Value *value);

  /// Clones an instruction.
  virtual Inst *Clone(Inst *inst);

  /// Fixes PHI nodes.
  void Fixup();

public:
  // Control flow.
  virtual Inst *Clone(CallInst *i);
  virtual Inst *Clone(TailCallInst *i);
  virtual Inst *Clone(InvokeInst *i);
  virtual Inst *Clone(TailInvokeInst *i);
  virtual Inst *Clone(ReturnInst *i);
  virtual Inst *Clone(JumpCondInst *i);
  virtual Inst *Clone(JumpIndirectInst *i);
  virtual Inst *Clone(JumpInst *i);
  virtual Inst *Clone(SwitchInst *i);
  virtual Inst *Clone(TrapInst *i);
  // Memory.
  virtual Inst *Clone(LoadInst *i);
  virtual Inst *Clone(StoreInst *i);
  virtual Inst *Clone(ExchangeInst *i);
  virtual Inst *Clone(SetInst *i);
  virtual Inst *Clone(VAStartInst *i);
  virtual Inst *Clone(FrameInst *i);
  // Ternary.
  virtual Inst *Clone(SelectInst *i);
  // Unary.
  virtual Inst *Clone(AbsInst *i)      { return CloneUnary<AbsInst>(i); }
  virtual Inst *Clone(NegInst *i)      { return CloneUnary<NegInst>(i); }
  virtual Inst *Clone(SqrtInst *i)     { return CloneUnary<SqrtInst>(i); }
  virtual Inst *Clone(SinInst *i)      { return CloneUnary<SinInst>(i); }
  virtual Inst *Clone(CosInst *i)      { return CloneUnary<CosInst>(i); }
  virtual Inst *Clone(SExtInst *i)     { return CloneUnary<SExtInst>(i); }
  virtual Inst *Clone(ZExtInst *i)     { return CloneUnary<ZExtInst>(i); }
  virtual Inst *Clone(FExtInst *i)     { return CloneUnary<FExtInst>(i); }
  virtual Inst *Clone(TruncInst *i)    { return CloneUnary<TruncInst>(i); }
  // Binary instructions.
  virtual Inst *Clone(CmpInst *i);
  virtual Inst *Clone(DivInst *i)      { return CloneBinary<DivInst>(i); }
  virtual Inst *Clone(RemInst *i)      { return CloneBinary<RemInst>(i); }
  virtual Inst *Clone(MulInst *i)      { return CloneBinary<MulInst>(i); }
  virtual Inst *Clone(AddInst *i)      { return CloneBinary<AddInst>(i); }
  virtual Inst *Clone(SubInst *i)      { return CloneBinary<SubInst>(i); }
  virtual Inst *Clone(AndInst *i)      { return CloneBinary<AndInst>(i); }
  virtual Inst *Clone(OrInst *i)       { return CloneBinary<OrInst>(i); }
  virtual Inst *Clone(SllInst *i)      { return CloneBinary<SllInst>(i); }
  virtual Inst *Clone(SraInst *i)      { return CloneBinary<SraInst>(i); }
  virtual Inst *Clone(SrlInst *i)      { return CloneBinary<SrlInst>(i); }
  virtual Inst *Clone(XorInst *i)      { return CloneBinary<XorInst>(i); }
  virtual Inst *Clone(RotlInst *i)     { return CloneBinary<RotlInst>(i); }
  virtual Inst *Clone(PowInst *i)      { return CloneBinary<PowInst>(i); }
  virtual Inst *Clone(CopySignInst *i) { return CloneBinary<CopySignInst>(i); }
  // Overflow.
  virtual Inst *Clone(AddUOInst *i)    { return CloneOverflow<AddUOInst>(i); }
  virtual Inst *Clone(MulUOInst *i)    { return CloneOverflow<MulUOInst>(i); }
  // Special.
  virtual Inst *Clone(MovInst *i);
  virtual Inst *Clone(UndefInst *i);
  virtual Inst *Clone(PhiInst *i);
  virtual Inst *Clone(ArgInst *i);

public:
  /// Clones a binary instruction.
  template<typename T> Inst *CloneBinary(BinaryInst *i)
  {
    return new T(i->GetType(), Map(i->GetLHS()), Map(i->GetRHS()));
  }

  /// Clones a unary instruction.
  template<typename T> Inst *CloneUnary(UnaryInst *i)
  {
    return new T(i->GetType(), Map(i->GetArg()));
  }

  /// Clones an overflow instruction.
  template<typename T> Inst *CloneOverflow(OverflowInst *i)
  {
    return new T(Map(i->GetLHS()), Map(i->GetRHS()));
  }

  /// Clones an argument list.
  template<typename T> std::vector<Inst *> CloneArgs(T *i)
  {
    std::vector<Inst *> args;
    for (auto *arg : i->args()) {
      args.push_back(Map(arg));
    }
    return args;
  }

protected:
  /// PHI instruction delayed fixups.
  llvm::SmallVector<std::pair<PhiInst *, PhiInst *>, 10> fixups_;
};
