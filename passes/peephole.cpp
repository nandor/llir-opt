// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include <llvm/ADT/Statistic.h>
#include <llvm/ADT/PostOrderIterator.h>
#include <llvm/ADT/SmallPtrSet.h>

#include "core/block.h"
#include "core/cast.h"
#include "core/cfg.h"
#include "core/func.h"
#include "core/prog.h"
#include "core/insts.h"
#include "passes/peephole.h"

#define DEBUG_TYPE "peephole"

STATISTIC(NumAddsSimplified, "Add instructions simplified");
STATISTIC(NumSubsSimplified, "Subtractions simplified");
STATISTIC(NumCastsEliminated, "Number of bit casts eliminated");
STATISTIC(NumCmpSimplified, "Number of comparisons simplified");



// -----------------------------------------------------------------------------
const char *PeepholePass::kPassID = "peephole";

// -----------------------------------------------------------------------------
const char *PeepholePass::GetPassName() const
{
  return "Peephole Optimisation";
}

// -----------------------------------------------------------------------------
bool PeepholePass::Run(Prog &prog)
{
  bool changed = false;
  for (auto &func : prog) {
    for (auto &block : func) {
      for (auto it = block.begin(); it != block.end(); ) {
        changed = Dispatch(*it++) || changed;
      }
    }
  }
  return changed;
}

// -----------------------------------------------------------------------------
bool PeepholePass::VisitAddInst(AddInst &inst)
{
  auto ty = inst.GetType();
  auto annot = inst.GetAnnots();

  if (auto mov = ::cast_or_null<MovInst>(inst.GetRHS())) {
    if (auto ci = ::cast_or_null<ConstantInt>(mov->GetArg())) {
      if (ci->GetValue().isNullValue()) {
        auto argTy = inst.GetLHS().GetType();
        if (argTy == ty && inst.GetLHS()->GetAnnots() == annot) {
          inst.replaceAllUsesWith(inst.GetLHS());
          inst.eraseFromParent();
        } else {
          auto *mov = new MovInst(ty, inst.GetLHS(), annot);
          inst.getParent()->AddInst(mov, &inst);
          inst.replaceAllUsesWith(mov);
          inst.eraseFromParent();
        }
        NumAddsSimplified++;
        return true;
      }
    }
  }
  if (auto mov = ::cast_or_null<MovInst>(inst.GetLHS())) {
    if (auto ci = ::cast_or_null<ConstantInt>(mov->GetArg())) {
      if (ci->GetValue().isNullValue()) {
        auto argTy = inst.GetRHS().GetType();
        if (argTy == ty && inst.GetRHS()->GetAnnots() == annot) {
          inst.replaceAllUsesWith(inst.GetRHS());
          inst.eraseFromParent();
        } else {
          auto *mov = new MovInst(ty, inst.GetRHS(), annot);
          inst.getParent()->AddInst(mov, &inst);
          inst.replaceAllUsesWith(mov);
          inst.eraseFromParent();
        }
        NumAddsSimplified++;
        return true;
      }
    }
  }
  return false;
}

// -----------------------------------------------------------------------------
bool PeepholePass::VisitSubInst(SubInst &inst)
{
  auto ty = inst.GetType();

  if (inst.GetLHS() == inst.GetRHS() && IsIntegerType(inst.GetType())) {
    auto *mov = new MovInst(ty, new ConstantInt(0), inst.GetAnnots());
    inst.getParent()->AddInst(mov, &inst);
    inst.replaceAllUsesWith(mov);
    inst.eraseFromParent();
    NumSubsSimplified++;
    return true;
  }
  return false;
}

// -----------------------------------------------------------------------------
bool PeepholePass::VisitStoreInst(StoreInst &inst)
{
  if (auto cast = ::cast_or_null<BitCastInst>(inst.GetValue())) {
    auto *newInst = new StoreInst(inst.GetAddr(), cast->GetArg(), inst.GetAnnots());
    inst.getParent()->AddInst(newInst, &inst);
    inst.eraseFromParent();
    NumCastsEliminated++;
    return true;
  }
  return false;
}

// -----------------------------------------------------------------------------
bool PeepholePass::VisitCmpInst(CmpInst &inst)
{
  auto *cmp = ::cast_or_null<CmpInst>(&inst);
  if (!cmp) {
    return false;
  }
  auto sll = ::cast_or_null<SllInst>(cmp->GetLHS());
  auto ref = ::cast_or_null<MovInst>(cmp->GetRHS());
  if (!sll || !ref) {
    return false;
  }

  auto one = ::cast_or_null<MovInst>(sll->GetRHS());
  auto zext = ::cast_or_null<ZExtInst>(sll->GetLHS());
  if (!one || !zext) {
    return false;
  }

  auto iref = ::cast_or_null<ConstantInt>(ref->GetArg());
  auto ione = ::cast_or_null<ConstantInt>(one->GetArg());
  if (!iref || !ione || !ione->GetValue().isOneValue()) {
    return false;
  }

  auto *block = inst.getParent();

  auto *newIRef = new ConstantInt(iref->GetValue().lshr(1));
  auto newRef = new MovInst(ref->GetType(), newIRef, ref->GetAnnots());
  block->AddInst(newRef, &inst);

  auto newCmp = new CmpInst(
      cmp->GetType(),
      zext,
      newRef,
      cmp->GetCC(),
      cmp->GetAnnots()
  );
  block->AddInst(newCmp, &inst);
  cmp->replaceAllUsesWith(newCmp);
  cmp->eraseFromParent();
  ++NumCmpSimplified;
  return true;
}

