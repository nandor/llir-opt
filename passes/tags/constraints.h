// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#pragma once

#include "core/adt/id.h"
#include "core/adt/union_find.h"
#include "core/target.h"
#include "core/inst_visitor.h"
#include "passes/tags/tagged_type.h"



namespace tags {

class RegisterAnalysis;

class ConstraintSolver : private InstVisitor<void> {
public:
  ConstraintSolver(RegisterAnalysis &analysis, const Target *target, Prog &prog);

  void Solve();

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
  void VisitGetInst(GetInst &i) override { ExactlyInt(i); }
  void VisitX86_RdTscInst(X86_RdTscInst &i) override { ExactlyInt(i); }
  void VisitMultiplyInst(MultiplyInst &i) override { ExactlyInt(i); }
  void VisitDivisionRemainderInst(DivisionRemainderInst &i) override { ExactlyInt(i); }
  void VisitByteSwapInst(ByteSwapInst &i) override { ExactlyInt(i); }
  void VisitBitCountInst(BitCountInst &i) override { ExactlyInt(i); }
  void VisitBitCastInst(BitCastInst &i) override { Equal(i, i.GetArg()); }
  void VisitVaStartInst(VaStartInst &i) override { ExactlyPointer(i.GetVAList()); }
  void VisitX86_FPUControlInst(X86_FPUControlInst &i) override { ExactlyPointer(i.GetAddr()); }
  void VisitTerminatorInst(TerminatorInst &i) override {}
  void VisitSetInst(SetInst &i) override {}
  void VisitX86_OutInst(X86_OutInst &i) override {}
  void VisitX86_WrMsrInst(X86_WrMsrInst &i) override {}
  void VisitX86_LidtInst(X86_LidtInst &i) override {}
  void VisitX86_LgdtInst(X86_LgdtInst &i) override {}
  void VisitX86_LtrInst(X86_LtrInst &i) override {}

  void VisitInst(Inst &i) override;

private:
  /// Bounds for a single variable.
  struct Constraint {
    TaggedType Min;
    TaggedType Max;

    Constraint(ID<Constraint> id)
      : Min(TaggedType::Unknown())
      , Max(TaggedType::PtrInt())
    {
    }
  };

  ID<Constraint> Find(Ref<Inst> a);
  Constraint *Map(Ref<Inst> a);

private:
  void Subset(Ref<Inst> from, Ref<Inst> to);
  void AtMost(Ref<Inst> a, const TaggedType &type);
  void AtLeast(Ref<Inst> a, const TaggedType &type);

  void AtMostInfer(Ref<Inst> arg);

  void AtMostPointer(Ref<Inst> arg) { AtMost(arg, TaggedType::Ptr()); }
  void ExactlyPointer(Ref<Inst> arg) { Exactly(arg, TaggedType::Ptr()); }
  void ExactlyYoung(Ref<Inst> arg) { Exactly(arg, TaggedType::Young()); }
  void ExactlyHeap(Ref<Inst> arg) { Exactly(arg, TaggedType::Heap()); }
  void ExactlyInt(Ref<Inst> arg) { Exactly(arg, TaggedType::Int()); }
  void ExactlyUndef(Ref<Inst> arg) { Exactly(arg, TaggedType::Undef()); }

  void Exactly(Ref<Inst> a, const TaggedType &type)
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
