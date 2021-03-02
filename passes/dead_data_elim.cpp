// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include <llvm/ADT/Statistic.h>
#include <llvm/ADT/SmallPtrSet.h>

#include "core/block.h"
#include "core/cast.h"
#include "core/data.h"
#include "core/prog.h"
#include "passes/dead_data_elim.h"


#define DEBUG_TYPE "dead-data-elim"

STATISTIC(NumDatasRemoved, "Segments erased");
STATISTIC(NumObjectsRemoved, "Objects erased");


// -----------------------------------------------------------------------------
const char *DeadDataElimPass::kPassID = "dead-data-elim";

// -----------------------------------------------------------------------------
const char *DeadDataElimPass::GetPassName() const
{
  return "Data Elimination";
}



// -----------------------------------------------------------------------------
bool DeadDataElimPass::Run(Prog &prog)
{
  bool changed = false;
  changed = RemoveExterns(prog) || changed;
  changed = RemoveObjects(prog) || changed;
  return changed;
}

// -----------------------------------------------------------------------------
bool DeadDataElimPass::RemoveExterns(Prog &prog)
{
  bool changed = false;
  for (auto it = prog.ext_begin(); it != prog.ext_end(); ) {
    Extern *ext = &*it++;
    if (ext->use_empty() && !ext->HasValue()) {
      ext->eraseFromParent();
      changed = true;
    }
  }
  return changed;
}

// -----------------------------------------------------------------------------
bool DeadDataElimPass::RemoveObjects(Prog &prog)
{
  bool changed = false;
  bool erased;
  do {
    erased = false;
    for (auto it = prog.data_begin(); it != prog.data_end(); ) {
      Data *data = &*it++;
      if (data->getName().startswith(".note")) {
        continue;
      }
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
          NumObjectsRemoved++;
          obj->eraseFromParent();
          changed = true;
          erased = true;
        }
      }

      if (data->empty()) {
        NumDatasRemoved++;
        data->eraseFromParent();
        changed = true;
        erased = true;
      }
    }
  } while (erased);
  return changed;
}
