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
      *use = nullptr;
    }
  }
  ext->eraseFromParent();
}

// -----------------------------------------------------------------------------
static bool IsInitFini(llvm::StringRef name)
{
  return name == "__init_array_start"
      || name == "__init_array_end"
      || name == "__fini_array_start"
      || name == "__fini_array_end";
}

// -----------------------------------------------------------------------------
using XtorMap = std::map<int, std::vector<Func *>>;

// -----------------------------------------------------------------------------
static Func *MergeXtors(Prog &prog, const char *name, const XtorMap &xtors)
{
  Func *xtor = new Func(name);
  prog.AddFunc(xtor);

  Block *end = new Block(".Lend");
  end->AddInst(new ReturnInst({}, {}));
  xtor->AddBlock(end);

  for (auto it = xtors.rbegin(); it != xtors.rend(); ++it) {
    for (auto ft = it->second.rbegin(); ft != it->second.rend(); ++ft) {
      Func *f = *ft;

      auto *next = &xtor->getEntryBlock();
      auto *block = new Block((".Lcall" + f->getName()).str());
      xtor->AddBlock(block, next);

      auto *mov = new MovInst(Type::I64, f, {});
      block->AddInst(mov);
      block->AddInst(new CallInst(
          {},
          mov,
          {},
          {},
          CallingConv::C,
          std::nullopt,
          next,
          {}
      ));
    }
  }
  return xtor;
}

// -----------------------------------------------------------------------------
bool LinkPass::Run(Prog &prog)
{
  bool changed = false;
  auto &cfg = GetConfig();
  if (cfg.Static) {
    XtorMap ctors, dtors;
    for (auto it = prog.xtor_begin(); it != prog.xtor_end(); ) {
      Xtor *xtor = &*it++;
      switch (xtor->GetKind()) {
        case Xtor::Kind::CTOR: {
          ctors[xtor->GetPriority()].push_back(xtor->GetFunc());
          xtor->eraseFromParent();
          continue;
        }
        case Xtor::Kind::DTOR: {
          dtors[xtor->GetPriority()].push_back(xtor->GetFunc());
          xtor->eraseFromParent();
          continue;
        }
      }
      llvm_unreachable("unknown xtor kind");
    }

    Func *ctor = MergeXtors(prog, "_init$merge", ctors);
    Func *dtor = MergeXtors(prog, "_fini$merge", dtors);

    for (auto it = prog.ext_begin(); it != prog.ext_end(); ) {
      Extern *ext = &*it++;
      // Replace _init/_fini with ctor/dtor.
      if (ext->getName() == "_init") {
        if (ctor) {
          ext->replaceAllUsesWith(ctor);
          ext->eraseFromParent();
        } else {
          ZeroExtern(ext);
        }
        changed = true;
        continue;
      }
      if (ext->getName() == "_fini") {
        if (ctor) {
          ext->replaceAllUsesWith(dtor);
          ext->eraseFromParent();
        } else {
          ZeroExtern(ext);
        }
        changed = true;
        continue;
      }
      // Resolve aliases.
      if (auto g = ::cast_or_null<Global>(ext->GetValue())) {
        ext->replaceAllUsesWith(g);
        if (ext->getName() == g->getName()) {
          ext->eraseFromParent();
          continue;
        }
      }
      // Delete externs with no uses.
      if (ext->use_empty() && !::cast_or_null<Constant>(ext->GetValue())) {
        ext->eraseFromParent();
        changed = true;
        continue;
      }
      // Weak symbols can be zeroed here.
      if (ext->IsWeak() || IsInitFini(ext->getName())) {
        ZeroExtern(ext);
        changed = true;
        continue;
      }
    }
    // Set the visibility of all non-entry symbols to local.
    for (Data &data : prog.data()) {
      for (Object &object : data) {
        for (Atom &atom : object) {
          atom.SetVisibility(Visibility::LOCAL);
          changed = true;
        }
      }
    }
    const std::string entry = cfg.Entry.empty() ? "_start" : cfg.Entry;
    for (Func &func : prog) {
      auto name = func.GetName();
      if (name != entry && name != "caml_garbage_collection") {
        func.SetVisibility(Visibility::LOCAL);
        changed = true;
      }
    }
  }
  return changed;
}

// -----------------------------------------------------------------------------
const char *LinkPass::GetPassName() const
{
  return "Linking";
}
