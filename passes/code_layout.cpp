// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include <llvm/ADT/PostOrderIterator.h>

#include "core/block.h"
#include "core/cast.h"
#include "core/cfg.h"
#include "core/func.h"
#include "core/prog.h"
#include "core/insts.h"
#include "passes/code_layout.h"

#define DEBUG_TYPE "code-layout"


// -----------------------------------------------------------------------------
const char *CodeLayoutPass::kPassID = DEBUG_TYPE;

// -----------------------------------------------------------------------------
bool CodeLayoutPass::Run(Prog &prog)
{
  return false;
}

// -----------------------------------------------------------------------------
const char *CodeLayoutPass::GetPassName() const
{
  return "Function Placement";
}
