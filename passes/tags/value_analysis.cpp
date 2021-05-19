// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include <llvm/Support/raw_ostream.h>

#include "core/prog.h"
#include "core/printer.h"
#include "passes/tags/value_analysis.h"

using namespace tags;



// ----------------------------------------------------------------------------
void ValueAnalysis::Solve()
{
  for (Func &func : prog_) {
    for (Block &block : func) {
      for (Inst &inst : block) {
        Dispatch(inst);
      }
    }
  }
}

// ----------------------------------------------------------------------------
void ValueAnalysis::Shift(Inst &inst)
{
}

// ----------------------------------------------------------------------------
void ValueAnalysis::VisitMovInst(MovInst &inst)
{
  auto arg = inst.GetArg();
  switch (arg->GetKind()) {
    case Value::Kind::INST: {
      llvm_unreachable("not implemented");
    }
    case Value::Kind::GLOBAL: {
      llvm_unreachable("not implemented");
    }
    case Value::Kind::CONST: {
      llvm_unreachable("not implemented");
    }
    case Value::Kind::EXPR: {
      llvm_unreachable("not implemented");
    }
  }
  llvm_unreachable("invalid value kind");
}

// ----------------------------------------------------------------------------
void ValueAnalysis::VisitInst(Inst &inst)
{
  std::string msg;
  llvm::raw_string_ostream os(msg);
  os << inst << "\n";
  llvm::report_fatal_error(msg.c_str());
}
