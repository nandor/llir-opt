// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include <llvm/ADT/PostOrderIterator.h>

#include "core/block.h"
#include "core/cast.h"
#include "core/cfg.h"
#include "core/func.h"
#include "core/prog.h"
#include "core/pass_manager.h"
#include "core/insts.h"
#include "passes/linearise.h"

#define DEBUG_TYPE "linearise"


// -----------------------------------------------------------------------------
const char *LinearisePass::kPassID = DEBUG_TYPE;

// -----------------------------------------------------------------------------
static bool IsSingleThreadedLinux(Prog &prog)
{
  llvm_unreachable("not implemented");
}

// -----------------------------------------------------------------------------
static bool IsSingleThreadedKVM(Prog &prog)
{
  // TODO: implement this.
  return true;
}

// -----------------------------------------------------------------------------
static bool IsSingleThreadedXen(Prog &prog)
{
  // TODO: implement this.
  return true;
}

// -----------------------------------------------------------------------------
static bool IsSingleThreaded(Prog &prog)
{
  return IsSingleThreadedLinux(prog) || IsSingleThreadedKVM(prog) ||
         IsSingleThreadedXen(prog);
}

// -----------------------------------------------------------------------------
bool LinearisePass::Run(Prog &prog)
{
  if (!GetConfig().Static || !IsSingleThreaded(prog)) {
    return false;
  }

  bool changed = false;
  return changed;
}

// -----------------------------------------------------------------------------
const char *LinearisePass::GetPassName() const
{
  return "Single-threaded linearisation";
}
