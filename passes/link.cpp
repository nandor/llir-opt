// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include <sstream>
#include <llvm/ADT/PostOrderIterator.h>
#include <llvm/ADT/SmallPtrSet.h>

#include "core/block.h"
#include "core/cast.h"
#include "core/cfg.h"
#include "core/func.h"
#include "core/prog.h"
#include "core/insts.h"
#include "passes/link.h"



// -----------------------------------------------------------------------------
const char *LinkPass::kPassID = "link";

// -----------------------------------------------------------------------------
void LinkPass::Run(Prog *prog)
{
  for (auto it = prog->xtor_begin(); it != prog->xtor_end(); ) {
    Xtor *xtor = &*it++;
    // TODO: combine ctors and dtors.
  }
}

// -----------------------------------------------------------------------------
const char *LinkPass::GetPassName() const
{
  return "Linking";
}
