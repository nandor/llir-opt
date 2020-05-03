// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#pragma once

#include "core/insts.h"
#include "passes/vtpta/constraint.h"



namespace vtpta {

class Builder {
public:
  void Build(const Func &func);

private:
  void BuildConstraint(const Func &func);

  void BuildFlow(const Inst &inst);

  template<typename T>
  SymExpr *BuildCall(const CallSite<T> &call);

  void BuildRet(const Inst &inst);
  void BuildArg(const ArgInst &inst);
  void BuildSelect(const SelectInst &inst);
  void BuildLoad(const LoadInst &inst);
  void BuildStore(const StoreInst &inst);
  void BuildXchg(const XchgInst &inst);
  void BuildVastart(const VAStartInst &inst);
  void BuildAlloca(const AllocaInst &inst);
  void BuildFrame(const FrameInst &inst);
  void BuildNeg(const NegInst &inst);
  void BuildTrunc(const TruncInst &inst);
  void BuildSext(const SExtInst &inst);
  void BuildZext(const ZExtInst &inst);
  void BuildFext(const FExtInst &inst);
  void BuildAdd(const AddInst &inst);
  void BuildSub(const SubInst &inst);
  void BuildCmp(const CmpInst &inst);
  void BuildMul(const MulInst &inst);
  void BuildMov(const MovInst &inst);
  void BuildPhi(const PhiInst &inst);
  void BuildUnknown(const Inst &inst);
};

}
