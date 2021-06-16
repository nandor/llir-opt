// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#pragma once

#include "core/adt/id.h"
#include "core/adt/bitset.h"
#include "core/adt/union_find.h"
#include "core/target.h"
#include "core/inst_visitor.h"
#include "passes/tags/tagged_type.h"



namespace tags {

class RegisterAnalysis;

enum class ConstraintType {
  BOT,
  // Pure integers.
  INT,
  // Pure pointers.
  PTR_BOT,
  YOUNG,
  HEAP,
  ADDR,
  PTR,
  FUNC,
  // Pointers or integers.
  ADDR_INT,
  PTR_INT,
  VAL,
};


/**
 * Implementation of the backtracking-based constraint solver.
 */
class ConstraintSolver : private InstVisitor<void> {
public:
  ConstraintSolver(RegisterAnalysis &analysis, const Target *target, Prog &prog);

  void Solve();
  void BuildConstraints();
  void CollapseEquivalences();

private:
  void VisitArgInst(ArgInst &i) override;
  void VisitCallSite(CallSite &i) override;
  void VisitLandingPadInst(LandingPadInst &i) override;

  void VisitSubInst(SubInst &i) override;
  void VisitAddInst(AddInst &i) override;
  void VisitAndInst(AndInst &i) override;
  void VisitOrInst(OrInst &i) override;
  void VisitXorInst(XorInst &i) override;
  void VisitCmpInst(CmpInst &i) override;
  void VisitSelectInst(SelectInst &i) override;
  void VisitPhiInst(PhiInst &i) override;

  void VisitMovInst(MovInst &i) override;
  void VisitExtensionInst(ExtensionInst &i) override;
  void VisitTruncInst(TruncInst &i) override;
  void VisitMemoryStoreInst(MemoryStoreInst &store) override;
  void VisitMemoryLoadInst(MemoryLoadInst &load) override;
  void VisitMemoryExchangeInst(MemoryExchangeInst &i) override;
  void VisitMemoryCompareExchangeInst(MemoryCompareExchangeInst &i) override;

  void VisitFrameInst(FrameInst &i) override { ExactlyPointer(i); }
  void VisitUndefInst(UndefInst &i) override { ExactlyUndef(i); }
  void VisitAllocaInst(AllocaInst &i) override { ExactlyPointer(i); }
  void VisitFloatInst(FloatInst &i) override { ExactlyInt(i); }
  void VisitShiftRightInst(ShiftRightInst &i) override { ExactlyInt(i); }
  void VisitSllInst(SllInst &i) override { ExactlyInt(i); }
  void VisitRotateInst(RotateInst &i) override { ExactlyInt(i); }
  void VisitCopySignInst(CopySignInst &i) override { ExactlyInt(i); }
  void VisitGetInst(GetInst &i) override { ExactlyInt(i); }
  void VisitX86_RdTscInst(X86_RdTscInst &i) override { ExactlyInt(i); }
  void VisitMultiplyInst(MultiplyInst &i) override { ExactlyInt(i); }
  void VisitDivisionRemainderInst(DivisionRemainderInst &i) override { ExactlyInt(i); }
  void VisitByteSwapInst(ByteSwapInst &i) override { ExactlyInt(i); }
  void VisitBitCountInst(BitCountInst &i) override { ExactlyInt(i); }
  void VisitBitCastInst(BitCastInst &i) override { Equal(i, i.GetArg()); }
  void VisitVaStartInst(VaStartInst &i) override { ExactlyPointer(i.GetVAList()); }
  void VisitX86_FPUControlInst(X86_FPUControlInst &i) override { AnyPointer(i.GetAddr()); }
  void VisitSyscallInst(SyscallInst &i) override { ExactlyInt(i); }
  void VisitCloneInst(CloneInst &i) override { ExactlyInt(i); }
  void VisitTerminatorInst(TerminatorInst &i) override {}
  void VisitSetInst(SetInst &i) override {}
  void VisitX86_OutInst(X86_OutInst &i) override {}
  void VisitX86_WrMsrInst(X86_WrMsrInst &i) override {}
  void VisitX86_LidtInst(X86_LidtInst &i) override {}
  void VisitX86_LgdtInst(X86_LgdtInst &i) override {}
  void VisitX86_LtrInst(X86_LtrInst &i) override {}
  void VisitX86_HltInst(X86_HltInst &i) override {}
  void VisitX86_PauseInst(X86_PauseInst &i) override {}
  void VisitX86_FnClExInst(X86_FnClExInst &i) override {}

  void VisitInst(Inst &i) override;

private:
  /// Bounds for a single variable.
  struct Constraint {
    ID<Constraint> Id;
    ConstraintType Min;
    ConstraintType Max;
    BitSet<Constraint> Subsets;

    Constraint(ID<Constraint> id)
      : Id(id)
      , Min(ConstraintType::BOT)
      , Max(ConstraintType::PTR_INT)
    {
    }

    void Union(Constraint &that);

    void dump(llvm::raw_ostream &os = llvm::errs());
  };

  ID<Constraint> Find(Ref<Inst> a);
  Constraint *Map(Ref<Inst> a);

private:
  void Subset(Ref<Inst> from, Ref<Inst> to);
  void AtMost(Ref<Inst> a, ConstraintType type);
  void AtLeast(Ref<Inst> a, ConstraintType type);

  void AtMostInfer(Ref<Inst> arg);

  void AtMostPointer(Ref<Inst> arg) { AtMost(arg, ConstraintType::PTR); }
  void ExactlyPointer(Ref<Inst> arg) { Exactly(arg, ConstraintType::PTR); }
  void ExactlyYoung(Ref<Inst> arg) { Exactly(arg, ConstraintType::YOUNG); }
  void ExactlyHeap(Ref<Inst> arg) { Exactly(arg, ConstraintType::HEAP); }
  void ExactlyInt(Ref<Inst> arg) { Exactly(arg, ConstraintType::INT); }
  void ExactlyUndef(Ref<Inst> arg) { Exactly(arg, ConstraintType::BOT); }
  void ExactlyFunc(Ref<Inst> arg) { Exactly(arg, ConstraintType::FUNC); }

  void AnyPointer(Ref<Inst> a)
  {
    AtLeast(a, ConstraintType::PTR_BOT);
    AtMost(a, ConstraintType::PTR);
  }

  void Exactly(Ref<Inst> a, const ConstraintType &type)
  {
    AtMost(a, type);
    AtLeast(a, type);
  }

  void Equal(Ref<Inst> a, Ref<Inst> b)
  {
    Subset(a, b);
    Subset(b, a);
  }

private:
  /// Reference to the analysis.
  RegisterAnalysis &analysis_;
  /// Reference to target info.
  const Target *target_;
  /// Program to analyse.
  Prog &prog_;

  /// Mapping from references to IDs.
  std::unordered_map<Ref<Inst>, ID<Constraint>> ids_;
  /// Union-find describing constraints for an ID.
  UnionFind<Constraint> union_;
};

}


// -----------------------------------------------------------------------------
bool operator<(tags::ConstraintType a, tags::ConstraintType b);

// -----------------------------------------------------------------------------
inline bool operator<=(tags::ConstraintType a, tags::ConstraintType b)
{
  return a == b || a < b;
}

// -----------------------------------------------------------------------------
llvm::raw_ostream &operator<<(llvm::raw_ostream &os, tags::ConstraintType type);

