// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include <llvm/ADT/PostOrderIterator.h>
#include <llvm/ADT/SmallPtrSet.h>

#include "core/block.h"
#include "core/cast.h"
#include "core/cfg.h"
#include "core/func.h"
#include "core/prog.h"
#include "core/insts.h"
#include "core/call_graph.h"
#include "core/pass_manager.h"
#include "passes/pre_eval.h"
#include "passes/pre_eval/tainted_objects.h"
#include "passes/pre_eval/symbolic_eval.h"
#include "passes/pre_eval/flow_graph.h"


#include <llvm/Support/GraphWriter.h>


// -----------------------------------------------------------------------------
const char *PreEvalPass::kPassID = "pre-eval";

// -----------------------------------------------------------------------------
bool PreEvalPass::Run(Prog &prog)
{
  auto &cfg = GetConfig();
  if (auto *entry = ::cast_or_null<Func>(prog.GetGlobal(cfg.Entry))) {
    llvm::ViewGraph(entry, "entry node");
  }
  return false;
}

// -----------------------------------------------------------------------------
const char *PreEvalPass::GetPassName() const
{
  return "Partial Pre-Evaluation";
}
