// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include <llvm/Support/raw_ostream.h>

#include "core/prog.h"
#include "core/printer.h"
#include "passes/tags/type_analysis.h"
#include "passes/tags/value_analysis.h"

using namespace tags;


#include <llvm/Support/GraphWriter.h>
#include "core/cfg.h"


// ----------------------------------------------------------------------------
void ValueAnalysis::Solve()
{
  for (Func &func : prog_) {
    for (Block &block : func) {
      for (Inst &inst : block) {
        if (auto *binary = ::cast_or_null<BinaryInst>(&inst)) {
          auto tl = types_.Find(binary->GetLHS());
          auto tr = types_.Find(binary->GetRHS());
          if (auto *shift = ::cast_or_null<ShiftRightInst>(binary)) {
            // TODO
          }
          if (auto *cmp = ::cast_or_null<CmpInst>(binary)) {
            if (tl.IsOddLike() && tr.IsOddLike()) {
            // TODO
            }
          }
        }
      }
    }
  }
}

// ----------------------------------------------------------------------------
void ValueAnalysis::dump(llvm::raw_ostream &os)
{
  class AnalysisPrinter : public Printer {
  public:
    AnalysisPrinter(llvm::raw_ostream &os, ValueAnalysis &values)
      : Printer(os)
      , values_(values)
      , types_(values_.types_)
    {
    }

    void PrintFuncHeader(const Func &func)
    {
    }

    void PrintInstHeader(const Inst &inst)
    {
      std::string str;
      llvm::raw_string_ostream os(str);
      for (unsigned i = 0, n = inst.GetNumRets(); i < n; ++i) {
        if (i != 0) {
          os << ", ";
        }
        os << types_.Find(inst.GetSubValue(i));
      }
      if (auto *mov = ::cast_or_null<const MovInst>(&inst)) {
        if (auto arg = ::cast_or_null<const Inst>(mov->GetArg())) {
          os << " REFINE ";
        }
      }

      os_ << str;
      for (unsigned i = str.size(); i < 30; ++i) {
        os_ << ' ';
      }
    }

  private:
    /// Reference to the value analysis.
    ValueAnalysis &values_;
    /// Reference to the analysis.
    TypeAnalysis &types_;
  };

  AnalysisPrinter(os, *this).Print(prog_);
}

// ----------------------------------------------------------------------------
void ValueAnalysis::VisitInst(Inst &inst)
{
  std::string msg;
  llvm::raw_string_ostream os(msg);
  os << inst << "\n";
  llvm::report_fatal_error(msg.c_str());
}
