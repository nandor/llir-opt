// This file if part of the genm-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include <cstdlib>

#include <memory>
#include <unordered_map>
#include <unordered_set>

#include <llvm/ADT/PostOrderIterator.h>
#include <llvm/ADT/SmallPtrSet.h>

#include "core/block.h"
#include "core/cast.h"
#include "core/cfg.h"
#include "core/constant.h"
#include "core/data.h"
#include "core/func.h"
#include "core/insts.h"
#include "core/prog.h"
#include "passes/vtpta.h"



// -----------------------------------------------------------------------------
class VTPTAContext final {
public:
  /// Initialises the type context.
  VTPTAContext(Prog *prog);

  /// Explores a function.
  void Explore(Func *func);

private:

};

// -----------------------------------------------------------------------------
VTPTAContext::VTPTAContext(Prog *prog)
{
}

// -----------------------------------------------------------------------------
void VTPTAContext::Explore(Func *func)
{
}

// -----------------------------------------------------------------------------
void VariantTypePointsToAnalysis::Run(Prog *prog)
{
  VTPTAContext graph(prog);

  for (auto &func : *prog) {
    // Include the function if it is extern.
    if (func.GetVisibility() == Visibility::EXTERN) {
      graph.Explore(&func);
      continue;
    }

    // Include the function if its address is taken.
    for (auto *user : func.users()) {
      if (!user) {
        graph.Explore(&func);
        break;
      }
    }
  }
}

// -----------------------------------------------------------------------------
const char *VariantTypePointsToAnalysis::GetPassName() const
{
  return "Variant Type Points-To Analysis";
}

// -----------------------------------------------------------------------------
char AnalysisID<VariantTypePointsToAnalysis>::ID;
