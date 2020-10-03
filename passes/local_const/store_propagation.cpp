// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include "passes/local_const/store_propagation.h"
#include "passes/local_const/context.h"



// -----------------------------------------------------------------------------
void StorePropagation::Set::Minus(const Kill &kill)
{
  for (auto it = defs_.begin(); it != defs_.end(); ) {
    if (kill.Elems.count(it->first)) {
      it = defs_.erase(it);
      continue;
    }
    if (kill.Allocs.Contains(it->first.first)) {
      it = defs_.erase(it);
      continue;
    }
    ++it;
  }
}

// -----------------------------------------------------------------------------
void StorePropagation::Set::Union(const Gen &gen)
{
  for (auto &elem : gen.Elems) {
    auto it = defs_.insert(elem);
    if (!it.second) {
      it.first->second = elem.second;
    }
  }
}

// -----------------------------------------------------------------------------
void StorePropagation::Set::Union(const Set &that)
{
  for (auto it = defs_.begin(); it != defs_.end(); ) {
    auto jt = that.defs_.find(it->first);
    if (jt == that.defs_.end()) {
      it = defs_.erase(it);
      continue;
    }
    if (it->second != jt->second) {
      it = defs_.erase(it);
      continue;
    }
    ++it;
  }
}

// -----------------------------------------------------------------------------
void StorePropagation::Set::dump(llvm::raw_ostream &os)
{
  for (const auto &[elem, inst] : defs_) {
    os << "[" << elem.first << ":" << elem.second << "]=" << inst << " ";
  }
}

// -----------------------------------------------------------------------------
void StorePropagation::Gen::Minus(const Kill &kill)
{
  for (auto it = Elems.begin(); it != Elems.end(); ) {
    if (kill.Elems.count(it->first)) {
      it = Elems.erase(it);
      continue;
    }
    if (kill.Allocs.Contains(it->first.first)) {
      it = Elems.erase(it);
      continue;
    }
    ++it;
  }
}

// -----------------------------------------------------------------------------
void StorePropagation::Gen::Union(const Gen &gen)
{
  for (const auto &elem : gen.Elems) {
    auto it = Elems.insert(elem);
    if (!it.second) {
      it.first->second = elem.second;
    }
  }
}

// -----------------------------------------------------------------------------
void StorePropagation::Gen::dump(llvm::raw_ostream &os)
{
  for (const auto &[elem, inst] : Elems) {
    os << "[" << elem.first << ":" << elem.second << "]=" << inst << " ";
  }
}

// -----------------------------------------------------------------------------
void StorePropagation::Kill::Union(const Kill &kill)
{
  Allocs.Union(kill.Allocs);
  for (const auto &elem : kill.Elems) {
    Elems.insert(elem);
  }
}

// -----------------------------------------------------------------------------
void StorePropagation::Kill::dump(llvm::raw_ostream &os)
{
  for (const auto &elem : Elems) {
    os << "[" << elem.first << ":" << elem.second << "] ";
  }
  for (const auto &alloc : Allocs) {
    os << "[" << alloc << "] ";
  }
}

// -----------------------------------------------------------------------------
void StorePropagation::Solver::Traverse(Inst *inst, const Set &reach)
{
  if (auto *ld = ::dyn_cast_or_null<LoadInst>(inst)) {
    if (auto *set = context_.GetNode(ld->GetAddr())) {
      // See if the load is from a unique address.
      std::optional<Element> elem;
      set->points_to_elem([&elem](LCAlloc *alloc, LCIndex idx) {
        if (elem) {
          elem = std::nullopt;
        } else {
          elem = { alloc->GetID(), idx };
        }
      });
      set->points_to_range([&elem](LCAlloc *alloc) {
        elem = std::nullopt;
      });
      if (!elem || !elem->second.IsField()) {
        return;
      }

      // Find a store which can be propagated.
      if (auto st = reach.Find(*elem)) {
        // Check if the argument can be propagated.
        auto val = st->GetVal();
        if (val->GetType(0) != ld->GetType()) {
          return;
        }

        ld->replaceAllUsesWith(val);
        ld->eraseFromParent();
      }
    }
  }
}


// -----------------------------------------------------------------------------
void StorePropagation::Solver::Build(Inst &inst)
{
  switch (inst.GetKind()) {
    default: {
      return;
    }
    // Reaching defs - everything is clobbered.
    // LVA - everithing is defined.
    case Inst::Kind::CALL:
    case Inst::Kind::TCALL:
    case Inst::Kind::INVOKE: {
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
    // The store instruction either defs or clobbers.
    // Reaching defs - def if store to unique pointer.
    // LVA - kill the set stored to.
    case Inst::Kind::ST: {
      auto &st = static_cast<StoreInst &>(inst);
      if (auto *addr = context_.GetNode(st.GetAddr())) {
        BuildStore(&st, addr);
      }
      return;
    }
    // Reaching defs - always clobber.
    // LVA - def and kill the pointer set.
    case Inst::Kind::XCHG: {
      if (auto *addr = context_.GetNode(static_cast<XchgInst &>(inst).GetAddr())) {
        BuildClobber(&inst, addr);
      }
      return;
    }
    // The vastart instruction clobbers.
    case Inst::Kind::VASTART: {
      if (auto *addr = context_.GetNode(static_cast<VAStartInst &>(inst).GetVAList())) {
        BuildClobber(&inst, addr);
      }
      return;
    }
    // Loads get a dummy node.
    case Inst::Kind::LD: {
      Info(&inst);
      return;
    }
  }
  llvm_unreachable("invalid instruction kind");
}

// -----------------------------------------------------------------------------
void StorePropagation::Solver::BuildCall(Inst *I)
{
  auto &kg = Info(I);
  BuildRoots(I, kg);
  BuildExtern(I, kg);
}

// -----------------------------------------------------------------------------
void StorePropagation::Solver::BuildLongJmp(Inst *I)
{
  BuildExtern(I, Info(I));
}

// -----------------------------------------------------------------------------
void StorePropagation::Solver::BuildAlloc(Inst *I)
{
  BuildRoots(I, Info(I));
}

// -----------------------------------------------------------------------------
void StorePropagation::Solver::BuildStore(StoreInst *st, LCSet *addr)
{
  const Type ty = st->GetVal()->GetType(0);
  auto &kg = Info(st);

  std::optional<std::pair<LCAlloc *, LCIndex>> elem;
  addr->points_to_elem([&elem, &kg, st, ty](LCAlloc *alloc, LCIndex idx) {
    auto allocID = alloc->GetID();
    if (!kg.Kill.Elems.empty() || !kg.Kill.Allocs.Empty()) {
      for (size_t i = 0, n = GetSize(ty); i < n; ++i) {
        if (auto off = alloc->Offset(idx, i)) {
          kg.Kill.Elems.insert({ allocID, *off });
        } else {
          kg.Kill.Allocs.Insert(allocID);
        }
      }
    } else if (elem) {
      for (size_t i = 0, n = GetSize(ty); i < n; ++i) {
        auto elemAllocID = elem->first->GetID();
        if (auto off = elem->first->Offset(elem->second, i)) {
          kg.Kill.Elems.insert({ elemAllocID, *off });
        } else {
          kg.Kill.Allocs.Insert(elemAllocID);
        }
        if (auto off = alloc->Offset(idx, i)) {
          kg.Kill.Elems.insert({ allocID, *off });
        } else {
          kg.Kill.Allocs.Insert(allocID);
        }
      }
      elem = std::nullopt;
    } else {
      elem = { alloc, idx };
    }
  });
  addr->points_to_range([&elem, &kg](LCAlloc *alloc) {
    kg.Kill.Allocs.Insert(alloc->GetID());
  });

  if (elem) {
    kg.Gen.Elems.emplace(Element{ elem->first->GetID(), elem->second }, st);
  }
}

// -----------------------------------------------------------------------------
void StorePropagation::Solver::BuildClobber(Inst *I, LCSet *addr)
{
  auto &kg = Info(I);
  addr->points_to_range([&kg](LCAlloc *alloc) {
    kg.Kill.Allocs.Insert(alloc->GetID());
  });
  addr->points_to_elem([&kg](LCAlloc *alloc, LCIndex index) {
    kg.Kill.Elems.insert({ alloc->GetID(), index });
  });
}

// -----------------------------------------------------------------------------
void StorePropagation::Solver::BuildExtern(Inst *I, InstInfo &kg)
{
  LCSet *ext = context_.Extern();
  ext->points_to_range([&kg](LCAlloc *alloc) {
    kg.Kill.Allocs.Insert(alloc->GetID());
  });
  ext->points_to_elem([&kg](LCAlloc *alloc, LCIndex index) {
    kg.Kill.Elems.insert({ alloc->GetID(), index });
  });
}

// -----------------------------------------------------------------------------
void StorePropagation::Solver::BuildRoots(Inst *I, InstInfo &kg)
{
  LCSet *root = context_.Root();
  root->points_to_range([&kg](LCAlloc *alloc) {
    kg.Kill.Allocs.Insert( alloc->GetID());
  });
  root->points_to_elem([&kg](LCAlloc *alloc, LCIndex index) {
    kg.Kill.Elems.insert({ alloc->GetID(), index });
  });

  if (auto *live = context_.GetLive(I)) {
    live->points_to_range([&kg](LCAlloc *alloc) {
      kg.Kill.Allocs.Insert(alloc->GetID());
    });
  }
}

