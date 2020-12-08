// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#pragma once

#include "core/insts.h"



/**
 * Helper class to build custom visitors for all IR instructions.
 */
template<typename T>
class InstVisitor {
public:
  virtual ~InstVisitor() {}

  T Dispatch(Inst *i)
  {
    switch (i->GetKind()) {
      #define GET_INST(kind, type, name, sort) \
        case Inst::Kind::kind: return Visit##type(static_cast<type##Inst *>(i));
      #include "instructions.def"
    }
    llvm_unreachable("invalid instruction kind");
  }

public:
  virtual T VisitInst(Inst *i) = 0;
  virtual T VisitConst(ConstInst *i) { return VisitInst(i); }
  virtual T VisitOperator(OperatorInst *i) { return VisitInst(i); }
  virtual T VisitUnary(UnaryInst *i) { return VisitOperator(i); }
  virtual T VisitBinary(BinaryInst *i) { return VisitOperator(i); }
  virtual T VisitOverflow(OverflowInst *i) { return VisitBinary(i); }
  virtual T VisitMemory(MemoryInst *i) { return VisitInst(i); }
  virtual T VisitControl(ControlInst *i) { return VisitInst(i); }
  virtual T VisitTerminator(TerminatorInst *i) { return VisitControl(i); }
  virtual T VisitCallSite(CallSite *i) { return VisitTerminator(i); }
  virtual T VisitX86_FPUControlInst(X86_FPUControlInst *i) { return VisitInst(i); }

public:
  #define GET_INST(kind, type, name, sort) \
    virtual T Visit##type(type##Inst *i) { return Visit##sort(i); }
  #include "instructions.def"
};
