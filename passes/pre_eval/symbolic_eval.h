// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#pragma once

#include "core/inst_visitor.h"
#include "passes/pre_eval/symbolic_context.h"
#include "passes/pre_eval/symbolic_value.h"
#include "passes/pre_eval/tainted_objects.h"



class SymbolicEval : public InstVisitor<void> {
public:
  SymbolicEval(SymbolicContext &ctx, TaintedObjects::Tainted &t);

  void Evaluate(Inst *inst);

private:
  void Visit(Inst *i) override { llvm_unreachable("missing visitor"); }

  void VisitMov(MovInst *i) override;
};
