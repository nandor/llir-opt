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
#include "core/insts.h"
#include "core/pass_manager.h"
#include "core/prog.h"
#include "passes/link.h"



// -----------------------------------------------------------------------------
const char *LinkPass::kPassID = "link";

// -----------------------------------------------------------------------------
static void ZeroExtern(Extern *ext)
{
  for (auto it = ext->use_begin(); it != ext->use_end(); ) {
    Use *use = &*it++;
    if (auto *inst = ::cast_or_null<MovInst>(use->getUser())) {
      *use = new ConstantInt(0);
    } else {
      llvm::report_fatal_error("not implemented");
    }
  }
  ext->eraseFromParent();
}

// -----------------------------------------------------------------------------
void LinkPass::Run(Prog *prog)
{
  Xtor *ctor = nullptr;
  Xtor *dtor = nullptr;
  for (auto it = prog->xtor_begin(); it != prog->xtor_end(); ) {
    Xtor *xtor = &*it++;
    switch (xtor->getKind()) {
      case Xtor::Kind::CTOR: {
        if (ctor) {
          llvm::report_fatal_error("duplicate ctor");
        }
        ctor = xtor;
        break;
      }
      case Xtor::Kind::DTOR: {
        if (dtor) {
          llvm::report_fatal_error("duplicate dtor");
        }
        dtor = xtor;
        break;
      }
    }
    llvm_unreachable("invalid xtor kind");
  }

  auto &cfg = GetConfig();
  if (cfg.Static) {
    const std::string entry = cfg.Entry.empty() ? "_start" : cfg.Entry;
    for (auto it = prog->ext_begin(); it != prog->ext_end(); ) {
      Extern *ext = &*it++;
      if (ext->use_empty()) {
        ext->eraseFromParent();
        continue;
      }

      // If there are no constructors/destructors, zero the arrays.
      if (!ctor && ext->getName() == "__init_array_start") {
        ZeroExtern(ext);
        continue;
      }
      if (!ctor && ext->getName() == "__init_array_end") {
        ZeroExtern(ext);
        continue;
      }
      if (!dtor && ext->getName() == "__fini_array_start") {
        ZeroExtern(ext);
        continue;
      }
      if (!dtor && ext->getName() == "__fini_array_end") {
        ZeroExtern(ext);
        continue;
      }

      // Weak symbols can be zeroed here.
      if (ext->IsWeak()) {
        ZeroExtern(ext);
        continue;
      }
    }

    for (Data &data : prog->data()) {
      for (Object &object : data) {
        for (Atom &atom : object) {
          atom.SetVisibility(Visibility::LOCAL);
        }
      }
    }

    for (Func &func : *prog) {
      auto name = func.GetName();
      if (name != entry && name != "caml_garbage_collection") {
        func.SetVisibility(Visibility::LOCAL);
      }
    }
  }
}

// -----------------------------------------------------------------------------
const char *LinkPass::GetPassName() const
{
  return "Linking";
}
