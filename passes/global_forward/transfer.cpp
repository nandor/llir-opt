// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include <llvm/ADT/Statistic.h>
#include <llvm/Support/Debug.h>

#include "passes/global_forward/forwarder.h"

#define DEBUG_TYPE "global-forward"

STATISTIC(NumLoadsFolded, "Loads folded");



// -----------------------------------------------------------------------------
static std::optional<std::pair<Object *, std::optional<uint64_t>>>
GetObject(Ref<Inst> inst)
{
  auto mov = ::cast_or_null<MovInst>(inst);
  if (!mov) {
    return std::nullopt;
  }
  auto arg = mov->GetArg();
  switch (arg->GetKind()) {
    case Value::Kind::CONST:
    case Value::Kind::INST: {
      return std::nullopt;
    }
    case Value::Kind::EXPR: {
      switch (::cast<Expr>(arg)->GetKind()) {
        case Expr::Kind::SYMBOL_OFFSET: {
          auto expr = ::cast<SymbolOffsetExpr>(arg);
          if (auto atom = ::cast_or_null<Atom>(expr->GetSymbol())) {
            auto *obj = atom->getParent();
            if (&*obj->begin() == &*atom) {
              return std::make_pair(obj, expr->GetOffset());
            }
            return std::make_pair(obj, std::nullopt);
          }
          return std::nullopt;
        }
      }
      llvm_unreachable("invalid expression kind");
    }
    case Value::Kind::GLOBAL: {
      if (auto atom = ::cast_or_null<Atom>(arg)) {
        auto *obj = atom->getParent();
        if (&*obj->begin() == &*atom) {
          return std::make_pair(obj, 0ull);
        }
        return std::make_pair(obj, std::nullopt);
      }
      return std::nullopt;
    }
  }
  llvm_unreachable("invalid argument kind");
}

// -----------------------------------------------------------------------------
static bool IsCompatible(Type a, Type b)
{
  return a == b
      || (a == Type::I64 && b == Type::V64)
      || (a == Type::V64 && b == Type::I64);
}

// -----------------------------------------------------------------------------
template <typename T>
Ref<T> GetConstant(Ref<Inst> arg)
{
  if (auto mov = ::cast_or_null<MovInst>(arg)) {
    return ::cast_or_null<T>(mov->GetArg());
  }
  return nullptr;
}


// -----------------------------------------------------------------------------
void GlobalForwarder::Approximator::VisitMovInst(MovInst &mov)
{
  // Record all symbols, except the ones which do not escape.
  state_.Escape(Funcs, Escaped, mov);
}

// -----------------------------------------------------------------------------
void GlobalForwarder::Approximator::VisitMemoryStoreInst(MemoryStoreInst &store)
{
  // Record a potential non-escaped symbol as mutated.
  size_t size = GetSize(store.GetValue().GetType());
  if (auto ptr = GetObject(store.GetAddr())) {
    Stored.Insert(state_.GetObjectID(ptr->first));
  }
}

// -----------------------------------------------------------------------------
void GlobalForwarder::Approximator::VisitMemoryLoadInst(MemoryLoadInst &load)
{
  // Record a potential non-escaped symbol and its closure as mutated.
  size_t size = GetSize(load.GetType());
  if (auto ptr = GetObject(load.GetAddr())) {
    auto id = state_.GetObjectID(ptr->first);
    auto &obj = *state_.objects_[id];
    Funcs.Union(obj.Funcs);
    Escaped.Union(obj.Objects);
    Loaded.Insert(id);
  }
}

// -----------------------------------------------------------------------------
void GlobalForwarder::Approximator::VisitCallSite(CallSite &site)
{
  if (auto *f = site.GetDirectCallee()) {
    auto &func = *state_.funcs_[state_.GetFuncID(*f)];
    Raises = Raises || func.Raises;
    Indirect = Indirect || func.Indirect;
    Funcs.Union(func.Funcs);
    Escaped.Union(func.Escaped);
    Loaded.Union(func.Loaded);
    Stored.Union(func.Stored);
  } else {
    Indirect = true;
  }
}

// -----------------------------------------------------------------------------
bool GlobalForwarder::Simplifier::VisitAddInst(AddInst &add)
{
  switch (auto ty = add.GetType()) {
    case Type::I8:
    case Type::I16:
    case Type::I32:
    case Type::I64:
    case Type::V64:
    case Type::I128: {
      auto vl = GetConstant<ConstantInt>(add.GetLHS());
      auto vr = GetConstant<ConstantInt>(add.GetRHS());
      if (!vl || !vr) {
        return false;
      }
      unsigned bits = GetBitWidth(ty);
      auto intl = vl->GetValue().sextOrTrunc(bits);
      auto intr = vr->GetValue().sextOrTrunc(bits);

      auto *v = new ConstantInt(intl + intr);
      auto *mov = new MovInst(ty, v, add.GetAnnots());
      LLVM_DEBUG(llvm::dbgs() << "\t\t\treplace: " << *mov << "\n");

      add.getParent()->AddInst(mov, &add);
      add.replaceAllUsesWith(mov);
      add.eraseFromParent();
      return true;
    }
    case Type::F32:
    case Type::F64:
    case Type::F80:
    case Type::F128: {
      auto vl = GetConstant<ConstantFloat>(add.GetLHS());
      auto vr = GetConstant<ConstantFloat>(add.GetRHS());
      if (!vl || !vr) {
        return false;
      }
      llvm_unreachable("not implemented");
    }
  }
  llvm_unreachable("invalid instruction type");
}

// -----------------------------------------------------------------------------
bool GlobalForwarder::Simplifier::VisitMovInst(MovInst &mov)
{
  state_.Escape(node_.Funcs, node_.Escaped, mov);
  return false;
}

// -----------------------------------------------------------------------------
bool GlobalForwarder::Simplifier::VisitMemoryStoreInst(MemoryStoreInst &store)
{
  auto ty = store.GetValue().GetType();
  if (auto ptr = GetObject(store.GetAddr())) {
    auto id = state_.GetObjectID(ptr->first);
    LLVM_DEBUG(llvm::dbgs()
        << "\t\tStore to " << ptr->first->begin()->getName()
        << ", " << id << "\n"
    );
    if (ptr->second) {
      auto off = *ptr->second;
      auto end = off + GetSize(ty);
      node_.Stored.Insert(id);

      auto &stores = node_.Stores[id];
      for (auto it = stores.begin(); it != stores.end(); ) {
        auto prevStart = it->first;
        auto prevEnd = prevStart + GetSize(it->second.first);
        if (prevEnd <= off || end <= prevStart) {
          ++it;
        } else {
          stores.erase(it++);
        }
      }

      auto v = store.GetValue();
      LLVM_DEBUG(llvm::dbgs() << "\t\t\tforward " << *v << "\n");
      stores.emplace(*ptr->second, std::make_pair(ty, v));
      reverse_.Store(id, off, end, &store);
    } else {
      node_.Stored.Insert(id);
      node_.Overwrite(id);
      reverse_.Store(id);
    }
  } else {
    node_.Overwrite(node_.Escaped);
    reverse_.Store(node_.Escaped);
  }
  return false;
}

// -----------------------------------------------------------------------------
bool GlobalForwarder::Simplifier::VisitMemoryLoadInst(MemoryLoadInst &load)
{
  if (auto ptr = GetObject(load.GetAddr())) {
    auto id = state_.GetObjectID(ptr->first);
    LLVM_DEBUG(llvm::dbgs()
        << "\t\tLoad from " << ptr->first->begin()->getName()
        << ", " << id << "\n"
    );
    auto imprecise = [&, this]
    {
      auto &obj = *state_.objects_[id];
      node_.Escaped.Union(obj.Objects);
      node_.Funcs.Union(obj.Funcs);
      return false;
    };

    auto &stores = node_.Stores[id];
    if (ptr->second) {
      auto off = *ptr->second;
      auto ty = load.GetType();
      auto end = off + GetSize(ty);
      LLVM_DEBUG(llvm::dbgs()
          << "\t\t\toffset: " << off << ", type: " << ty << "\n"
      );
      // The offset is known - try to record the stored value.
      auto it = stores.find(off);
      if (it != stores.end()) {
        // Forwarding a previous store to load from.
        auto [storeTy, storeValue] = it->second;
        if (auto mov = ::cast_or_null<MovInst>(storeValue)) {
          auto movArg = mov->GetArg();
          if (movArg->IsConstant()) {
            if (IsCompatible(ty, storeTy)) {
              auto *mov = new MovInst(ty, movArg, load.GetAnnots());
              LLVM_DEBUG(llvm::dbgs() << "\t\t\treplace: " << *mov << "\n");
              load.getParent()->AddInst(mov, &load);
              load.replaceAllUsesWith(mov);
              load.eraseFromParent();
              return true;
            } else {
              llvm_unreachable("not implemented");
            }
          }
        }
      } else if (!node_.Stored.Contains(id)) {
        // Value not yet mutated, load from static data.
        if (auto *v = ptr->first->Load(off, ty)) {
          ++NumLoadsFolded;
          auto *mov = new MovInst(ty, v, load.GetAnnots());
          LLVM_DEBUG(llvm::dbgs() << "\t\t\treplace: " << *mov << "\n");
          load.getParent()->AddInst(mov, &load);
          load.replaceAllUsesWith(mov);
          load.eraseFromParent();
          return true;
        }
      }
      // Cannot forward - non-static move.
      reverse_.Load(id, off, end);
      return imprecise();
    } else {
      reverse_.Load(id);
      return imprecise();
    }
  } else {
    // Imprecise load, all pointees should have already been tainted.
    reverse_.Load(node_.Escaped);
    return false;
  }
}

// -----------------------------------------------------------------------------
bool GlobalForwarder::Simplifier::VisitMemoryExchangeInst(MemoryExchangeInst &xchg)
{
  auto ty = xchg.GetValue().GetType();
  if (auto ptr = GetObject(xchg.GetAddr())) {
    auto id = state_.GetObjectID(ptr->first);
    auto &obj = *state_.objects_[id];

    node_.Escaped.Union(obj.Objects);
    node_.Funcs.Union(obj.Funcs);

    node_.Stored.Insert(id);
    node_.Stores[id].clear();

    if (ptr->second) {
      auto off = *ptr->second;
      auto end = off + GetSize(ty);

      reverse_.Load(id, off, end);
      reverse_.Store(id, off, end);
    } else {
      reverse_.Load(id);
      reverse_.Store(id);
    }

    return false;
  } else {
    node_.Overwrite(node_.Escaped);
    reverse_.Load(node_.Escaped);
    reverse_.Store(node_.Escaped);
    return false;
  }
}
