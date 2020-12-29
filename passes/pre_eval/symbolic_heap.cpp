// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include "core/atom.h"
#include "core/object.h"
#include "core/global.h"
#include "passes/pre_eval/symbolic_value.h"
#include "passes/pre_eval/symbolic_heap.h"



// -----------------------------------------------------------------------------
SymbolicObject::SymbolicObject(Object &object)
  : object_(object)
  , align_(object.begin()->GetAlignment().value_or(llvm::Align(1)))
{
  if (object.size() == 1) {
    Atom &atom = *object.begin();
    start_.emplace(&atom, std::make_pair(0u, 0u));
    size_ = atom.GetByteSize();
    for (auto it = atom.begin(); it != atom.end(); ) {
      Item *item = &*it++;
      switch (item->GetKind()) {
        case Item::Kind::INT8:
        case Item::Kind::INT16:
        case Item::Kind::INT32: {
          llvm_unreachable("not implemented");
        }
        case Item::Kind::INT64: {
          buckets_.push_back(SymbolicValue::Integer(
              llvm::APInt(64, item->GetInt64(), true)
          ));
          continue;
        }
        case Item::Kind::EXPR: {
          llvm_unreachable("not implemented");
        }
        case Item::Kind::FLOAT64: {
          llvm_unreachable("not implemented");
        }
        case Item::Kind::SPACE: {
          llvm_unreachable("not implemented");
        }
        case Item::Kind::STRING: {
          llvm_unreachable("not implemented");
        }
      }
      llvm_unreachable("invalid item kind");
    }
  } else {
    llvm_unreachable("not implemented");
  }
}

// -----------------------------------------------------------------------------
SymbolicObject::~SymbolicObject()
{
}

// -----------------------------------------------------------------------------
void SymbolicObject::StoreAtom(
    Atom *a,
    int64_t offset,
    const SymbolicValue &val,
    Type type)
{
  // This only works for single-atom objects.
  unsigned bucket = offset / 8;
  if (offset % 8 != 0) {
    llvm_unreachable("not implemented");
  }

  switch (type) {
    case Type::I64:
    case Type::V64: {
      buckets_[bucket] = val;
      return;
    }
    case Type::I8:
    case Type::I16:
    case Type::I32:
    case Type::I128:
    case Type::F32:
    case Type::F64:
    case Type::F80:
    case Type::F128: {
      llvm_unreachable("not implemented");
    }
  }
  llvm_unreachable("invalid type");
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
void SymbolicHeap::Store(
    const SymbolicPointer &addr,
    const SymbolicValue &val,
    Type type)
{
  if (auto ptr = addr.ToPrecise()) {
    StoreGlobal(ptr->first, ptr->second, val, type);
  } else {
    llvm::errs() << addr << "\n";
    llvm_unreachable("Store");
  }
}

// -----------------------------------------------------------------------------
void SymbolicHeap::StoreGlobal(
    Global *g,
    int64_t offset,
    const SymbolicValue &value,
    Type type)
{
  switch (g->GetKind()) {
    case Global::Kind::FUNC:
    case Global::Kind::BLOCK: {
      // Undefined behaviour - stores to these locations should not occur.
      return;
    }
    case Global::Kind::EXTERN: {
      // Over-approximate a store to an arbitrary external pointer.
      return StoreExtern(value);
    }
    case Global::Kind::ATOM: {
      // Precise store to an atom.
      return StoreAtom(static_cast<Atom *>(g), offset, value, type);
    }
  }
}

// -----------------------------------------------------------------------------
void SymbolicHeap::StoreAtom(
    Atom *a,
    int64_t offset,
    const SymbolicValue &value,
    Type type)
{
  Object &parent = *a->getParent();
  auto it = objects_.emplace(&parent, nullptr);
  if (it.second) {
    it.first->second.reset(new SymbolicObject(parent));
  }
  return it.first->second->StoreAtom(a, offset, value, type);
}

// -----------------------------------------------------------------------------
void SymbolicHeap::StoreImprecise(const SymbolicPointer &addr)
{

}

// -----------------------------------------------------------------------------
void SymbolicHeap::StoreExtern(const SymbolicValue &value)
{
  llvm_unreachable("not implemented");
}
