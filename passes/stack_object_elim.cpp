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
#include "passes/stack_object_elim.h"



// -----------------------------------------------------------------------------
const char *StackObjectElimPass::kPassID = "stack-object-elim";

// -----------------------------------------------------------------------------
void StackObjectElimPass::Run(Prog *prog)
{
  for (Func &func : *prog) {
    // Find the referenced frame indices.
    llvm::DenseSet<unsigned> used;
    for (Block &block : func) {
      for (Inst &inst : block) {
        if (auto *frame = ::dyn_cast_or_null<FrameInst>(&inst)) {
          used.insert(frame->GetObject());
        }
      }
    }
    // Find the unused objects.
    llvm::DenseSet<unsigned> unused;
    for (auto &object : func.objects()) {
      if (used.find(object.Index) == used.end()) {
        unused.insert(object.Index);
      }
    }
    // Delete the object.
    for (unsigned index : unused) {
      func.RemoveStackObject(index);
    }
  }
}

// -----------------------------------------------------------------------------
const char *StackObjectElimPass::GetPassName() const
{
  return "Stack Object Elimination";
}
