// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#pragma once

#include <queue>
#include <unordered_set>
#include <unordered_map>
#include <llvm/Support/raw_ostream.h>

#include "core/inst_visitor.h"
#include "core/target.h"
#include "passes/tags/tagged_type.h"



namespace tags {
class TypeAnalysis;

class ValueAnalysis : public InstVisitor<void> {
public:
  ValueAnalysis(TypeAnalysis &analysis, Prog &prog)
    : analysis_(analysis)
    , prog_(prog)
  {
    Solve();
  }

  void Solve();

protected:
  void Shift(Inst &i);

protected:
  void VisitMovInst(MovInst &i) override;
  void VisitGetInst(GetInst &i) override { Shift(i); }

  // Instructions with no effect.
  void VisitTerminatorInst(TerminatorInst &i) override {}
  void VisitSetInst(SetInst &i) override {}
  void VisitX86_OutInst(X86_OutInst &i) override {}
  void VisitX86_WrMsrInst(X86_WrMsrInst &i) override {}
  void VisitX86_LidtInst(X86_LidtInst &i) override {}
  void VisitX86_LgdtInst(X86_LgdtInst &i) override {}
  void VisitX86_LtrInst(X86_LtrInst &i) override {}
  void VisitX86_FPUControlInst(X86_FPUControlInst &i) override {}

  void VisitInst(Inst &inst) override;

private:
  TypeAnalysis &analysis_;
  Prog &prog_;
};

}
