// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#pragma once

#include <unordered_map>

#include "core/cast.h"
#include "core/insts.h"
#include "core/insts/binary.h"
#include "core/insts/call.h"
#include "core/insts/control.h"
#include "core/insts/memory.h"
#include "core/insts/unary.h"

class Inst;
class Block;
class Global;
class Prog;



/**
 * Helper class which clones instructions.
 */
class CloneVisitor {
public:
  /// Destroys the visitor.
  virtual ~CloneVisitor();

public:
  /// Maps a block to a new one.
  virtual Block *Map(Block *block) { return block; }
  /// Maps a block to a new one.
  virtual Func *Map(Func *func) { return func; }
  /// Maps a block to a new one.
  virtual Extern *Map(Extern *ext) { return ext; }
  /// Maps an atom to a new one.
  virtual Atom *Map(Atom *atom) { return atom; }
  /// Maps a constant to a new one.
  virtual Constant *Map(Constant *constant) { return constant; }
  /// Maps a global to a potentially new one.
  virtual Global *Map(Global *global);
  /// Maps an expression to a potentially new one.
  virtual Expr *Map(Expr *expr);
  /// Clones an annotation.
  virtual AnnotSet Annot(const Inst *inst);

public:
  /// Maps an instruction reference to a new one.
  virtual Ref<Inst> Map(Ref<Inst> inst) = 0;

public:
  /// Clones an instruction.
  virtual Inst *Clone(Inst *inst);
  /// Fixes PHI nodes.
  void Fixup();

public:
  // Control flow.
  virtual Inst *Clone(CallInst *i);
  virtual Inst *Clone(TailCallInst *i);
  virtual Inst *Clone(InvokeInst *i);
  virtual Inst *Clone(SyscallInst *i);
  virtual Inst *Clone(CloneInst *i);
  virtual Inst *Clone(ReturnInst *i);
  virtual Inst *Clone(JumpCondInst *i);
  virtual Inst *Clone(RaiseInst *i);
  virtual Inst *Clone(JumpInst *i);
  virtual Inst *Clone(SwitchInst *i);
  virtual Inst *Clone(TrapInst *i);
  // Memory.
  virtual Inst *Clone(LoadInst *i);
  virtual Inst *Clone(StoreInst *i);
  virtual Inst *Clone(VAStartInst *i);
  virtual Inst *Clone(FrameInst *i);
  virtual Inst *Clone(AllocaInst *i);
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
  virtual Inst *Clone(XExtInst *i)     { return CloneUnary<XExtInst>(i); }
  virtual Inst *Clone(FExtInst *i)     { return CloneUnary<FExtInst>(i); }
  virtual Inst *Clone(TruncInst *i)    { return CloneUnary<TruncInst>(i); }
  virtual Inst *Clone(ExpInst *i)      { return CloneUnary<ExpInst>(i); }
  virtual Inst *Clone(Exp2Inst *i)     { return CloneUnary<Exp2Inst>(i); }
  virtual Inst *Clone(LogInst *i)      { return CloneUnary<LogInst>(i); }
  virtual Inst *Clone(Log2Inst *i)     { return CloneUnary<Log2Inst>(i); }
  virtual Inst *Clone(Log10Inst *i)    { return CloneUnary<Log10Inst>(i); }
  virtual Inst *Clone(FCeilInst *i)    { return CloneUnary<FCeilInst>(i); }
  virtual Inst *Clone(FFloorInst *i)   { return CloneUnary<FFloorInst>(i); }
  virtual Inst *Clone(PopCountInst *i) { return CloneUnary<PopCountInst>(i); }
  virtual Inst *Clone(BSwapInst *i)    { return CloneUnary<BSwapInst>(i); }
  virtual Inst *Clone(CLZInst *i)      { return CloneUnary<CLZInst>(i); }
  virtual Inst *Clone(CTZInst *i)      { return CloneUnary<CTZInst>(i); }
  // Binary instructions.
  virtual Inst *Clone(CmpInst *i);
  virtual Inst *Clone(UDivInst *i)     { return CloneBinary<UDivInst>(i); }
  virtual Inst *Clone(SDivInst *i)     { return CloneBinary<SDivInst>(i); }
  virtual Inst *Clone(URemInst *i)     { return CloneBinary<URemInst>(i); }
  virtual Inst *Clone(SRemInst *i)     { return CloneBinary<SRemInst>(i); }
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
  virtual Inst *Clone(SubUOInst *i)    { return CloneOverflow<SubUOInst>(i); }
  virtual Inst *Clone(AddSOInst *i)    { return CloneOverflow<AddSOInst>(i); }
  virtual Inst *Clone(MulSOInst *i)    { return CloneOverflow<MulSOInst>(i); }
  virtual Inst *Clone(SubSOInst *i)    { return CloneOverflow<SubSOInst>(i); }
  // Special.
  virtual Inst *Clone(MovInst *i);
  virtual Inst *Clone(UndefInst *i);
  virtual Inst *Clone(PhiInst *i);
  virtual Inst *Clone(ArgInst *i);
  virtual Inst *Clone(SetInst *i);
  // X86 hardware.
  virtual Inst *Clone(X86_XchgInst *i);
  virtual Inst *Clone(X86_CmpXchgInst *i);
  virtual Inst *Clone(X86_RdtscInst *i);
  virtual Inst *Clone(X86_FnClExInst *i);
  virtual Inst *Clone(X86_FnStCwInst *i) { return CloneX86_FPUControl<X86_FnStCwInst>(i); }
  virtual Inst *Clone(X86_FnStSwInst *i) { return CloneX86_FPUControl<X86_FnStSwInst>(i); }
  virtual Inst *Clone(X86_FnStEnvInst *i) { return CloneX86_FPUControl<X86_FnStEnvInst>(i); }
  virtual Inst *Clone(X86_FLdCwInst *i) { return CloneX86_FPUControl<X86_FLdCwInst>(i); }
  virtual Inst *Clone(X86_FLdEnvInst *i) { return CloneX86_FPUControl<X86_FLdEnvInst>(i); }
  virtual Inst *Clone(X86_LdmXCSRInst *i) { return CloneX86_FPUControl<X86_LdmXCSRInst>(i); }
  virtual Inst *Clone(X86_StmXCSRInst *i) { return CloneX86_FPUControl<X86_StmXCSRInst>(i); }

protected:
  /// Maps a value to a potentially new one.
  Ref<Value> Map(Ref<Value> value);

  /// Clones a binary instruction.
  template<typename T>
  Inst *CloneBinary(BinaryInst *i)
  {
    return new T(
        i->GetType(),
        Map(i->GetLHS()),
        Map(i->GetRHS()),
        Annot(i)
    );
  }

  /// Clones a unary instruction.
  template<typename T>
  Inst *CloneUnary(UnaryInst *i)
  {
    return new T(
        i->GetType(),
        Map(i->GetArg()),
        Annot(i)
    );
  }

  /// Clones an overflow instruction.
  template<typename T>
  Inst *CloneOverflow(OverflowInst *i)
  {
    return new T(
        i->GetType(),
        Map(i->GetLHS()),
        Map(i->GetRHS()),
        Annot(i)
    );
  }

  /// Clones an X86 FPU control instruction.
  template<typename T>
  Inst *CloneX86_FPUControl(X86_FPUControlInst *i)
  {
    return new T(Map(i->GetAddr()), Annot(i));
  }

  /// Clones an argument list.
  template<typename T>
  std::vector<Ref<Inst>> CloneArgs(T *i)
  {
    std::vector<Ref<Inst>> args;
    for (Ref<Inst> arg : i->args()) {
      args.push_back(Map(arg));
    }
    return args;
  }

protected:
  /// PHI instruction delayed fixups.
  llvm::SmallVector<std::pair<PhiInst *, PhiInst *>, 10> fixups_;
};


/**
 * Clone a program and return the copy of a specific instruction.
 */
std::pair<std::unique_ptr<Prog>, Inst *> Clone(Prog &oldProg, Inst *inst);


/**
 * Clone a program and return the copy of a specific instruction.
 */
template<typename T>
std::pair<std::unique_ptr<Prog>, T *> CloneT(Prog &oldProg, T *inst)
{
  auto &&[prog, newInst] = Clone(oldProg, inst);
  return { std::move(prog), static_cast<T *>(newInst) };
}


/**
 * Helper method to clone a program.
 */
std::unique_ptr<Prog> Clone(Prog &oldProg);
