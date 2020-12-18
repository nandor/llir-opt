// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include <queue>

#include <llvm/ADT/PostOrderIterator.h>
#include <llvm/ADT/SmallPtrSet.h>
#include <llvm/ADT/SCCIterator.h>

#include "core/block.h"
#include "core/cast.h"
#include "core/cfg.h"
#include "core/func.h"
#include "core/prog.h"
#include "core/insts.h"
#include "core/object_graph.h"
#include "passes/const_global.h"



// -----------------------------------------------------------------------------
const char *ConstGlobalPass::kPassID = "const-global";

// -----------------------------------------------------------------------------
static bool IsReadOnly(const Atom &atom)
{
  std::queue<const User *> qu;
  std::queue<const Inst *> qi;
  for (const User *user : atom.users()) {
    qu.push(user);
  }
  while (!qu.empty()) {
    const User *u = qu.front();
    qu.pop();

    if (auto *inst = ::cast_or_null<const Inst>(u)) {
      qi.push(inst);
      continue;
    }
    if (auto *expr = ::cast_or_null<const Expr>(u)) {
      for (const User *user : expr->users()) {
        qu.push(user);
      }
      continue;
    }
  }


  while (!qi.empty()) {
    const Inst *i = qi.front();
    qi.pop();
    switch (i->GetKind()) {
      default: return false;
      case Inst::Kind::LOAD: {
        continue;
      }
      case Inst::Kind::MOV:
      case Inst::Kind::ADD:
      case Inst::Kind::SUB: {
        for (const User *user : i->users()) {
          if (auto *inst = ::cast_or_null<const Inst>(user)) {
            qi.push(inst);
          }
        }
        continue;
      }
    }

  }
  return true;
}

// -----------------------------------------------------------------------------
void ConstGlobalPass::Run(Prog *prog)
{
  ObjectGraph og(*prog);
  std::vector<Object *> readOnlyObjects;
  for (auto it = llvm::scc_begin(&og); !it.isAtEnd(); ++it) {
    bool isReadOnly = true;
    for (auto *node : *it) {
      if (auto *obj = node->GetObject()) {
        for (const Atom &atom : *obj) {
          if (!IsReadOnly(atom)) {
            isReadOnly = false;
            break;
          }
        }
        if (!isReadOnly) {
          break;
        }
      }
    }
    if (isReadOnly) {
      for (auto *node : *it) {
        if (auto *obj = node->GetObject()) {
          if (obj->getParent()->IsConstant()) {
            continue;
          }
          readOnlyObjects.push_back(obj);
        }
      }
    }
  }

  for (Object *object : readOnlyObjects) {
    if (object->getParent()->getName() == ".data") {
      Data *rodata = prog->GetData(".const");
      object->removeFromParent();
      rodata->AddObject(object);
    }
  }
}

// -----------------------------------------------------------------------------
const char *ConstGlobalPass::GetPassName() const
{
  return "Trivial Global Elimination";
}
