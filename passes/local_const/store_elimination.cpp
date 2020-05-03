// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include "passes/local_const/store_elimination.h"
#include "passes/local_const/context.h"

// -----------------------------------------------------------------------------
void StoreElimination::Set::Minus(const KillGen &kill)
{
  for (auto elem : kill.Elems) {
    elems_.erase(elem);
  }
}

// -----------------------------------------------------------------------------
void StoreElimination::Set::Union(const KillGen &gen)
{
  for (auto elem : gen.Elems) {
    elems_.insert(elem);
    allocs_.Insert(elem.first);
  }
  allocs_.Union(gen.Allocs);
}

// -----------------------------------------------------------------------------
void StoreElimination::Set::Union(const Set &that)
{
  for (auto elem : that.elems_) {
    elems_.insert(elem);
    allocs_.Insert(elem.first);
  }
  allocs_.Union(that.allocs_);
}

// -----------------------------------------------------------------------------
void StoreElimination::Set::dump(llvm::raw_ostream &os)
{
  llvm_unreachable("StoreElimination::Set");
}

// -----------------------------------------------------------------------------
void StoreElimination::KillGen::Minus(const KillGen &that)
{
  for (auto elem : that.Elems) {
    Elems.erase(elem);
  }
}

// -----------------------------------------------------------------------------
void StoreElimination::KillGen::Union(const KillGen &that)
{
  for (auto elem : that.Elems) {
    Elems.insert(elem);
    Allocs.Insert(elem.first);
  }
  Allocs.Union(that.Allocs);
}

// -----------------------------------------------------------------------------
void StoreElimination::KillGen::dump(llvm::raw_ostream &os)
{
  llvm_unreachable("StoreElimination::KillGen");
}

// -----------------------------------------------------------------------------
void StoreElimination::Solver::Traverse(Inst *inst, const Set &live)
{
  if (auto *store = ::dyn_cast_or_null<StoreInst>(inst)) {
    if (auto *set = context_.GetNode(store->GetAddr())) {
      // Check if the store writes to a live location.
      bool isLive = false;
      set->points_to_elem([&](LCAlloc *alloc, uint64_t index) {
        auto allocID = alloc->GetID();
        isLive |= live.Contains(allocID, index);
        isLive |= live.Contains(allocID);
      });
      set->points_to_range([&](LCAlloc *alloc) {
        isLive |= live.Contains(alloc->GetID());
      });

      // If not, erase it.
      if (!isLive) {
        store->eraseFromParent();
      }
    }
  }
}

// -----------------------------------------------------------------------------
void StoreElimination::Solver::Build(Inst &inst)
{
  switch (inst.GetKind()) {
    default: {
      return;
    }
    // Reaching defs - everything is clobbered.
    // LVA - everithing is defined.
    case Inst::Kind::CALL:
    case Inst::Kind::TCALL:
    case Inst::Kind::INVOKE:
    case Inst::Kind::TINVOKE: {
      if (auto *movInst = ::dyn_cast_or_null<MovInst>(inst.Op<0>())) {
        if (auto *callee = ::dyn_cast_or_null<Global>(movInst->GetArg())) {
          const auto &name = callee->getName();
          if (name.substr(0, 10) == "caml_alloc") {
            BuildAlloc(&inst);
            return;
          }
          if (name == "malloc") {
            BuildAlloc(&inst);
            return;
          }
          if (name == "longjmp") {
            BuildLongJmp(&inst);
            return;
          }
        }
      }
      BuildCall(&inst);
      return;
    }
    // Reaching defs - nothing is clobbered.
    // LVA - Result of ret is defined.
    case Inst::Kind::JI:
    case Inst::Kind::RET: {
      if (auto *set = context_.GetNode(&inst)) {
        BuildGen(&inst, set);
      }
      BuildGen(&inst, context_.Root());
      BuildGen(&inst, context_.Extern());
      return;
    }
    // The store instruction either defs or clobbers.
    // Reaching defs - def if store to unique pointer.
    // LVA - kill the set stored to.
    case Inst::Kind::ST: {
      auto &st = static_cast<StoreInst &>(inst);
      auto *addr = context_.GetNode(st.GetAddr());
      assert(addr && "missing pointer for set");
      BuildStore(&st, addr);
      return;
    }
    // Reaching defs - always clobber.
    // LVA - def and kill the pointer set.
    case Inst::Kind::XCHG: {
      auto *addr = context_.GetNode(static_cast<XchgInst &>(inst).GetAddr());
      assert(addr && "missing set for xchg");
      BuildClobber(&inst, addr);
      return;
    }
    // The vastart instruction clobbers.
    case Inst::Kind::VASTART: {
      auto *addr = context_.GetNode(static_cast<VAStartInst &>(inst).GetVAList());
      assert(addr && "missing address for vastart");
      BuildClobber(&inst, addr);
      return;
    }
    // Reaching defs - no clobber.
    // LVA - def the pointer set.
    case Inst::Kind::LD: {
      if (auto *addr = context_.GetNode(static_cast<LoadInst &>(inst).GetAddr())) {
        BuildGen(&inst, addr);
      }
      return;
    }
  }
  llvm_unreachable("invalid instruction kind");
}

// -----------------------------------------------------------------------------
void StoreElimination::Solver::BuildCall(Inst *I)
{
  auto &kg = Info(I);
  BuildRoots(I, kg);
  BuildExtern(I, kg);
  BuildReturn(I, kg);
}

// -----------------------------------------------------------------------------
void StoreElimination::Solver::BuildLongJmp(Inst *I)
{
  auto &kg = Info(I);
  BuildExtern(I, kg);
  for (auto &obj : func_.objects()) {
    kg.Gen.Allocs.Insert(context_.Frame(obj.Index)->GetID());
  }
}

// -----------------------------------------------------------------------------
void StoreElimination::Solver::BuildAlloc(Inst *I)
{
  BuildRoots(I, Info(I));
}

// -----------------------------------------------------------------------------
void StoreElimination::Solver::BuildStore(StoreInst *st, LCSet *addr)
{
  auto &kg = Info(st);
  addr->points_to_elem([&kg, st](LCAlloc *alloc, LCIndex idx) {
    kg.Kill.Elems.insert({ alloc->GetID(), idx });
  });
  addr->points_to_range([&kg](LCAlloc *alloc) {
    kg.Kill.Allocs.Insert(alloc->GetID());
  });
}

// -----------------------------------------------------------------------------
void StoreElimination::Solver::BuildClobber(Inst *I, LCSet *addr)
{
  auto &kg = Info(I);
  addr->points_to_range([&kg](LCAlloc *alloc) {
    auto allocID = alloc->GetID();
    kg.Gen.Allocs.Insert(allocID);
  });
  addr->points_to_elem([&kg](LCAlloc *alloc, uint64_t index) {
    Element elem{ alloc->GetID(), index };
    kg.Gen.Elems.insert(elem);
    kg.Kill.Elems.insert(elem);
  });
}

// -----------------------------------------------------------------------------
void StoreElimination::Solver::BuildGen(Inst *I, LCSet *addr)
{
  auto &kg = Info(I);
  addr->points_to_range([&kg](LCAlloc *alloc) {
    kg.Gen.Allocs.Insert(alloc->GetID());
  });
  addr->points_to_elem([&kg](LCAlloc *alloc, uint64_t index) {
    kg.Gen.Elems.insert({ alloc->GetID(), index });
  });
}

// -----------------------------------------------------------------------------
void StoreElimination::Solver::BuildExtern(Inst *I, InstInfo &kg)
{
  LCSet *ext = context_.Extern();
  ext->points_to_range([&kg](LCAlloc *alloc) {
    auto allocID = alloc->GetID();
    kg.Gen.Allocs.Insert(allocID);
  });
  ext->points_to_elem([&kg](LCAlloc *alloc, LCIndex index) {
    auto allocID = alloc->GetID();
    kg.Gen.Elems.insert({ allocID, index });
  });
}

// -----------------------------------------------------------------------------
void StoreElimination::Solver::BuildRoots(Inst *I, InstInfo &kg)
{
  LCSet *root = context_.Root();
  root->points_to_range([&kg](LCAlloc *alloc) {
    kg.Gen.Allocs.Insert(alloc->GetID());
  });
  root->points_to_elem([&kg](LCAlloc *alloc, uint64_t index) {
    Element elem{ alloc->GetID(), index };
    kg.Kill.Elems.insert(elem);
    kg.Gen.Elems.insert(elem);
  });

  if (auto *live = context_.GetLive(I)) {
    live->points_to_range([&kg](LCAlloc *alloc) {
      kg.Gen.Allocs.Insert(alloc->GetID());
    });
  }
}

// -----------------------------------------------------------------------------
void StoreElimination::Solver::BuildReturn(Inst *I, InstInfo &kg)
{
  if (LCSet *ret = context_.GetNode(I)) {
    ret->points_to_range([&kg](LCAlloc *alloc) {
      kg.Gen.Allocs.Insert(alloc->GetID());
    });
    ret->points_to_elem([&kg](LCAlloc *alloc, uint64_t index) {
      kg.Gen.Elems.insert({ alloc->GetID(), index });
    });
  }
}
