// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include <llvm/Support/Debug.h>

#include "core/atom.h"
#include "core/object.h"
#include "core/global.h"
#include "core/func.h"
#include "passes/pre_eval/symbolic_value.h"
#include "passes/pre_eval/symbolic_heap.h"

#define DEBUG_TYPE "pre-eval"



// -----------------------------------------------------------------------------
SymbolicFrame::SymbolicFrame(Func &func, unsigned index)
  : func_(func)
  , index_(index)
  , valid_(true)
{
  for (auto &object : func.objects()) {
    objects_.emplace(
        object.Index,
        std::make_unique<SymbolicFrameObject>(
            *this,
            object.Index,
            object.Size,
            object.Alignment
        )
    );
  }
}

// -----------------------------------------------------------------------------
SymbolicHeap::SymbolicHeap(Prog &prog)
{
}

// -----------------------------------------------------------------------------
SymbolicHeap::~SymbolicHeap()
{
}


// -----------------------------------------------------------------------------
void SymbolicHeap::EnterFrame(Func &func)
{
  frames_.emplace_back(func, frames_.size());
}

// -----------------------------------------------------------------------------
void SymbolicHeap::LeaveFrame(Func &func)
{
  frames_.rbegin()->Leave();
}

// -----------------------------------------------------------------------------
bool SymbolicHeap::Store(
    const SymbolicPointer &addr,
    const SymbolicValue &val,
    Type type)
{
  LLVM_DEBUG(llvm::dbgs()
      << "Storing " << val << ":" << type << " to " << addr << "\n"
  );
  auto begin = addr.address_begin();
  if (std::next(begin) == addr.address_end()) {
    switch (begin->GetKind()) {
      case SymbolicAddress::Kind::GLOBAL: {
        auto &a = begin->AsGlobal();
        return StoreGlobal(a.Symbol, a.Offset, val, type);
      }
      case SymbolicAddress::Kind::GLOBAL_RANGE: {
        auto &a = begin->AsGlobalRange();
        return StoreGlobalImprecise(a.Symbol, val, type);
      }
      case SymbolicAddress::Kind::FRAME: {
        auto &a = begin->AsFrame();
        auto &object = GetFrame(a.Frame, a.Object);
        return object.Store(a.Offset, val, type);
      }
      case SymbolicAddress::Kind::FRAME_RANGE: {
        auto &a = begin->AsFrameRange();
        auto &object = GetFrame(a.Frame, a.Object);
        return object.StoreImprecise(val, type);
      }
      case SymbolicAddress::Kind::FUNC: {
        llvm_unreachable("not implemented");
      }
    }
    llvm_unreachable("invalid address kind");
  } else {
    bool c = false;
    for (auto &address : addr.addresses()) {
      switch (address.GetKind()) {
        case SymbolicAddress::Kind::GLOBAL: {
          auto &a = address.AsGlobal();
          c = StoreGlobalImprecise(a.Symbol, a.Offset, val, type) || c;
          continue;
        }
        case SymbolicAddress::Kind::GLOBAL_RANGE: {
          auto &a = address.AsGlobalRange();
          c = StoreGlobalImprecise(a.Symbol, val, type) || c;
          continue;
        }
        case SymbolicAddress::Kind::FRAME: {
          auto &a = begin->AsFrame();
          auto &object = GetFrame(a.Frame, a.Object);
          c = object.StoreImprecise(a.Offset, val, type) || c;
          continue;
        }
        case SymbolicAddress::Kind::FRAME_RANGE: {
          auto &a = begin->AsFrameRange();
          auto &object = GetFrame(a.Frame, a.Object);
          c = object.StoreImprecise(val, type) || c;
          continue;
        }
        case SymbolicAddress::Kind::FUNC: {
          llvm_unreachable("not implemented");
        }
      }
      llvm_unreachable("invalid address kind");
    }
    return c;
  }
}

// -----------------------------------------------------------------------------
SymbolicValue SymbolicHeap::Load(const SymbolicPointer &addr, Type type)
{
  LLVM_DEBUG(llvm::dbgs() << "Loading " << type << " from " << addr << "\n");

  std::optional<SymbolicValue> value;
  for (auto &address : addr.addresses()) {
    switch (address.GetKind()) {
      case SymbolicAddress::Kind::GLOBAL: {
        auto &a = address.AsGlobal();
        auto v = LoadGlobal(a.Symbol, a.Offset, type);
        if (value) {
          value = value->LUB(v);
        } else {
          value = v;
        }
        continue;
      }
      case SymbolicAddress::Kind::GLOBAL_RANGE: {
        llvm_unreachable("not implemented");
      }
      case SymbolicAddress::Kind::FRAME: {
        llvm_unreachable("not implemented");
      }
      case SymbolicAddress::Kind::FRAME_RANGE: {
        llvm_unreachable("not implemented");
      }
      case SymbolicAddress::Kind::FUNC: {
        llvm_unreachable("not implemented");
      }
    }
    llvm_unreachable("invalid address kind");
  }
  assert(value && "missing address");
  return *value;
}

// -----------------------------------------------------------------------------
bool SymbolicHeap::StoreGlobal(
    Global *g,
    int64_t offset,
    const SymbolicValue &value,
    Type type)
{
  switch (g->GetKind()) {
    case Global::Kind::FUNC:
    case Global::Kind::BLOCK: {
      // Undefined behaviour - stores to these locations should not occur.
      return false;
    }
    case Global::Kind::EXTERN: {
      // Over-approximate a store to an arbitrary external pointer.
      return StoreExtern(value);
    }
    case Global::Kind::ATOM: {
      // Precise store to an atom.
      auto *atom = static_cast<Atom *>(g);
      auto &object = GetObject(atom);
      return object.Store(atom, offset, value, type);
    }
  }
}

// -----------------------------------------------------------------------------
SymbolicValue SymbolicHeap::LoadGlobal(Global *g, int64_t offset, Type type)
{
  switch (g->GetKind()) {
    case Global::Kind::FUNC:
    case Global::Kind::BLOCK: {
      // Undefined behaviour - stores to these locations should not occur.
      llvm_unreachable("not implemented");
    }
    case Global::Kind::EXTERN: {
      // Over-approximate a store to an arbitrary external pointer.
      return LoadExtern();
    }
    case Global::Kind::ATOM: {
      // Precise store to an atom.
      auto *atom = static_cast<Atom *>(g);
      auto &object = GetObject(atom);
      return object.Load(atom, offset, type);
    }
  }
}

// -----------------------------------------------------------------------------
bool SymbolicHeap::StoreGlobalImprecise(
    Global *g,
    int64_t offset,
    const SymbolicValue &value,
    Type type)
{
  switch (g->GetKind()) {
    case Global::Kind::FUNC:
    case Global::Kind::BLOCK: {
      // Undefined behaviour - stores to these locations should not occur.
      return false;
    }
    case Global::Kind::EXTERN: {
      // Over-approximate a store to an arbitrary external pointer.
      return StoreExtern(value);
    }
    case Global::Kind::ATOM: {
      // Precise store to an atom.
      auto *atom = static_cast<Atom *>(g);
      auto &object = GetObject(atom);
      return object.StoreImprecise(atom, offset, value, type);
    }
  }
}

// -----------------------------------------------------------------------------
bool SymbolicHeap::StoreGlobalImprecise(
    Global *g,
    const SymbolicValue &value,
    Type type)
{
  switch (g->GetKind()) {
    case Global::Kind::FUNC:
    case Global::Kind::BLOCK: {
      // Undefined behaviour - stores to these locations should not occur.
      return false;
    }
    case Global::Kind::EXTERN: {
      // Over-approximate a store to an arbitrary external pointer.
      return StoreExtern(value);
    }
    case Global::Kind::ATOM: {
      // Precise store to an atom.
      auto &object = GetObject(static_cast<Atom *>(g));
      return object.StoreImprecise(value, type);
    }
  }
}

// -----------------------------------------------------------------------------
SymbolicDataObject &SymbolicHeap::GetObject(Atom *atom)
{
  auto &parent = *atom->getParent();
  auto it = objects_.emplace(&parent, nullptr);
  if (it.second) {
    it.first->second.reset(new SymbolicDataObject(parent));
  }
  return *it.first->second;
}

// -----------------------------------------------------------------------------
bool SymbolicHeap::StoreExtern(const SymbolicValue &value)
{
  llvm_unreachable("not implemented");
}

// -----------------------------------------------------------------------------
SymbolicValue SymbolicHeap::LoadExtern()
{
  llvm_unreachable("not implemented");
}
