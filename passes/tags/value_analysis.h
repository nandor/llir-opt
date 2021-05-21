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
  ValueAnalysis(TypeAnalysis &types, Prog &prog)
    : types_(types)
    , prog_(prog)
  {
    Solve();
  }

  /// Dump the results of the analysis.
  void dump(llvm::raw_ostream &os = llvm::errs());

protected:
  void Solve();

  // Instructions with no effect.
  void VisitInst(Inst &inst) override;

private:
  TypeAnalysis &types_;
  Prog &prog_;
};

}
