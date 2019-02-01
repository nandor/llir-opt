// This file if part of the genm-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include <llvm/ADT/SmallPtrSet.h>
#include "core/block.h"
#include "core/func.h"
#include "core/prog.h"
#include "core/insts.h"
#include "passes/global_data_elim.h"



// -----------------------------------------------------------------------------
void GlobalDataElimPass::Run(Prog *prog)
{
  for (auto &func : *prog) {

  }
}

// -----------------------------------------------------------------------------
const char *GlobalDataElimPass::GetPassName() const
{
  return "Global Data Elimination Pass";
}
