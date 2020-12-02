// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include <llvm/ADT/SmallPtrSet.h>

#include "core/block.h"
#include "core/cast.h"
#include "core/data.h"
#include "core/prog.h"
#include "passes/dead_data_elim.h"



// -----------------------------------------------------------------------------
const char *DeadDataElimPass::kPassID = "dead-data-elim";

// -----------------------------------------------------------------------------
void DeadDataElimPass::Run(Prog *prog)
{
  // Remove dead externs.
  for (auto it = prog->ext_begin(); it != prog->ext_end(); ) {
    Extern *ext = &*it++;
    if (ext->use_empty() && !ext->HasAlias()) {
      ext->eraseFromParent();
    }
  }

  // Remove dead data segments.
  for (auto it = prog->data_begin(); it != prog->data_end(); ) {
    Data *data = &*it++;
    for (auto jt = data->begin(); jt != data->end(); ) {
      Object *obj = &*jt++;
      bool isReferenced = false;
      for (Atom &atom : *obj) {
        if (!atom.use_empty() || !atom.IsLocal()) {
          isReferenced = true;
          break;
        }
      }
      if (!isReferenced) {
        obj->eraseFromParent();
      }
    }

    if (data->empty()) {
      data->eraseFromParent();
    }
  }
}

// -----------------------------------------------------------------------------
const char *DeadDataElimPass::GetPassName() const
{
  return "Data Elimination";
}

