// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include <llvm/Support/Debug.h>

#include "core/atom.h"
#include "core/object.h"
#include "core/global.h"
#include "core/func.h"
#include "core/inst.h"
#include "passes/pre_eval/symbolic_value.h"
#include "passes/pre_eval/symbolic_context.h"

#define DEBUG_TYPE "pre-eval"



// -----------------------------------------------------------------------------
SymbolicContext::SymbolicContext(Prog &prog)
{
}

// -----------------------------------------------------------------------------
SymbolicContext::SymbolicContext(const SymbolicContext &that)
{
  for (auto &[g, object] : that.objects_) {
    objects_.emplace(g, std::make_unique<SymbolicDataObject>(*object));
  }
  for (auto &frame : that.frames_) {
    frames_.emplace_back(frame);
  }
}

// -----------------------------------------------------------------------------
SymbolicContext::~SymbolicContext()
{
}


// -----------------------------------------------------------------------------
unsigned SymbolicContext::EnterFrame(
    Func &func,
    llvm::ArrayRef<SymbolicValue> args)
{
  unsigned frame = frames_.size();
  frames_.emplace_back(func, frame, args);
  return frame;
}

// -----------------------------------------------------------------------------
unsigned SymbolicContext::EnterFrame(llvm::ArrayRef<Func::StackObject> objects)
{
  unsigned frame = frames_.size();
  frames_.emplace_back(frame, objects);
  return frame;
}


// -----------------------------------------------------------------------------
void SymbolicContext::LeaveFrame(Func &func)
{
  frames_.rbegin()->Leave();
}

// -----------------------------------------------------------------------------
bool SymbolicContext::Store(
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
SymbolicValue SymbolicContext::Load(const SymbolicPointer &addr, Type type)
{
  LLVM_DEBUG(llvm::dbgs() << "Loading " << type << " from " << addr << "\n");

  std::optional<SymbolicValue> value;
  auto merge = [&value] (const SymbolicValue &v)
  {
    if (value) {
      value = value->LUB(v);
    } else {
      value = v;
    }
  };

  for (auto &address : addr.addresses()) {
    switch (address.GetKind()) {
      case SymbolicAddress::Kind::GLOBAL: {
        auto &a = address.AsGlobal();
        merge(LoadGlobal(a.Symbol, a.Offset, type));
        continue;
      }
      case SymbolicAddress::Kind::GLOBAL_RANGE: {
        llvm_unreachable("not implemented");
      }
      case SymbolicAddress::Kind::FRAME: {
        auto &a = address.AsFrame();
        auto &object = GetFrame(a.Frame, a.Object);
        merge(object.Load(a.Offset, type));
        continue;
      }
      case SymbolicAddress::Kind::FRAME_RANGE: {
        auto &a = address.AsFrame();
        auto &object = GetFrame(a.Frame, a.Object);
        merge(object.LoadImprecise(type));
        continue;
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
bool SymbolicContext::StoreGlobal(
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
SymbolicValue SymbolicContext::LoadGlobal(Global *g, int64_t offset, Type type)
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
bool SymbolicContext::StoreGlobalImprecise(
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
bool SymbolicContext::StoreGlobalImprecise(
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
SymbolicDataObject &SymbolicContext::GetObject(Atom *atom)
{
  auto &parent = *atom->getParent();
  auto it = objects_.emplace(&parent, nullptr);
  if (it.second) {
    it.first->second.reset(new SymbolicDataObject(parent));
  }
  return *it.first->second;
}

// -----------------------------------------------------------------------------
bool SymbolicContext::StoreExtern(const SymbolicValue &value)
{
  llvm_unreachable("not implemented");
}

// -----------------------------------------------------------------------------
SymbolicValue SymbolicContext::LoadExtern()
{
  llvm_unreachable("not implemented");
}
