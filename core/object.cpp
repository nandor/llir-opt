// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include "core/cast.h"
#include "core/constant.h"
#include "core/data.h"
#include "core/object.h"
#include "core/printer.h"
#include "core/prog.h"



// -----------------------------------------------------------------------------
Object::~Object()
{
}

// -----------------------------------------------------------------------------
void Object::removeFromParent()
{
  getParent()->remove(this->getIterator());
}

// -----------------------------------------------------------------------------
void Object::eraseFromParent()
{
  getParent()->erase(this->getIterator());
}

// -----------------------------------------------------------------------------
void Object::AddAtom(Atom *atom, Atom *before)
{
  if (before == nullptr) {
    atoms_.push_back(atom);
  } else {
    atoms_.insert(before->getIterator(), atom);
  }
}

// -----------------------------------------------------------------------------
void Object::dump(llvm::raw_ostream &os) const
{
  Printer(os).Print(*this);
}

// -----------------------------------------------------------------------------
static std::optional<std::pair<Atom::iterator, int64_t>>
GetItem(Object *object, uint64_t offset)
{
  auto *data = object->getParent();
  auto *atom = &*object->begin();

  uint64_t i;
  uint64_t itemOff;
  auto it = atom->begin();
  for (i = 0; it != atom->end() && i + it->GetSize() <= offset; ++it) {
    if (it == atom->end()) {
      // TODO: jump to next atom.
      return std::nullopt;
    }
    i += it->GetSize();
  }
  if (it == atom->end()) {
    return std::nullopt;
  }

  itemOff = offset - i;
  return std::make_pair(it, itemOff);
}

// -----------------------------------------------------------------------------
static Value *LoadInt(Atom::iterator it, unsigned off, unsigned size)
{
  switch (it->GetKind()) {
    case Item::Kind::INT8: {
      if (size == 1) {
        return new ConstantInt(it->GetInt8());
      }
      break;
    }
    case Item::Kind::INT16: {
      if (size == 2) {
        return new ConstantInt(it->GetInt16());
      }
      break;
    }
    case Item::Kind::INT32: {
      if (size == 4) {
        return new ConstantInt(it->GetInt32());
      }
      break;
    }
    case Item::Kind::INT64: {
      if (size == 8) {
        return new ConstantInt(it->GetInt64());
      }
      break;
    }
    case Item::Kind::STRING: {
      if (size == 1) {
        return new ConstantInt(it->getString()[off]);
      }
      break;
    }
    case Item::Kind::SPACE: {
      if (off + size <= it->GetSpace()) {
        return new ConstantInt(0);
      }
      break;
    }
    case Item::Kind::FLOAT64: {
      break;
    }
    case Item::Kind::EXPR: {
      auto *expr = it->GetExpr();
      switch (expr->GetKind()) {
        case Expr::Kind::SYMBOL_OFFSET: {
          auto *sym = static_cast<SymbolOffsetExpr *>(expr);
          if (size == 8) {
            if (sym->GetOffset()) {
              return expr;
            } else {
              return sym->GetSymbol();
            }
          }
          break;
        }
      }
      return nullptr;
    }
  }
  // TODO: conversion based on endianness.
  return nullptr;
}

// -----------------------------------------------------------------------------
Value *Object::Load(uint64_t offset, Type type)
{
  auto it = GetItem(this, offset);
  if (!it) {
    return nullptr;
  }

  switch (type) {
    case Type::I8: {
      return LoadInt(it->first, it->second, 1);
    }
    case Type::I16: {
      return LoadInt(it->first, it->second, 2);
    }
    case Type::I32: {
      return LoadInt(it->first, it->second, 4);
    }
    case Type::I64:
    case Type::V64: {
      return LoadInt(it->first, it->second, 8);
    }
    case Type::F32:
    case Type::F64:
    case Type::I128:
    case Type::F80:
    case Type::F128: {
      llvm_unreachable("not implemented");
    }
  }
  llvm_unreachable("invalid type");
}

// -----------------------------------------------------------------------------
static bool StoreExpr(Atom::iterator it, unsigned off, Expr *expr)
{
  switch (it->GetKind()) {
    case Item::Kind::INT8: {
      llvm_unreachable("not implemented");
    }
    case Item::Kind::INT16: {
      llvm_unreachable("not implemented");
    }
    case Item::Kind::INT32: {
      llvm_unreachable("not implemented");
    }
    case Item::Kind::EXPR:
    case Item::Kind::INT64:
    case Item::Kind::FLOAT64: {
      auto *item = new Item(expr);
      it->getParent()->AddItem(item, &*it);
      it->eraseFromParent();
      return true;
    }
    case Item::Kind::STRING: {
      llvm_unreachable("not implemented");
    }
    case Item::Kind::SPACE: {
      llvm_unreachable("not implemented");
    }
  }
  llvm_unreachable("invalid item kind");
}

// -----------------------------------------------------------------------------
static bool StoreInt(
    Atom::iterator it,
    unsigned off,
    Type type,
    const APInt &value)
{
  switch (it->GetKind()) {
    case Item::Kind::INT8: {
      if (type == Type::I8) {
        auto *item = new Item(static_cast<int8_t>(value.getSExtValue()));
        it->getParent()->AddItem(item, &*it);
        it->eraseFromParent();
        return true;
      }
      llvm_unreachable("not implemented");
    }
    case Item::Kind::INT16: {
      llvm_unreachable("not implemented");
    }
    case Item::Kind::INT32: {
      llvm_unreachable("not implemented");
    }
    case Item::Kind::INT64: {
      if (type == Type::I64 || type == Type::V64) {
        auto *item = new Item(static_cast<int64_t>(value.getSExtValue()));
        it->getParent()->AddItem(item, &*it);
        it->eraseFromParent();
        return true;
      }
      llvm_unreachable("not implemented");
    }
    case Item::Kind::STRING: {
      llvm_unreachable("not implemented");
    }
    case Item::Kind::SPACE: {
      llvm_unreachable("not implemented");
    }
    case Item::Kind::FLOAT64: {
      llvm_unreachable("not implemented");
    }
    case Item::Kind::EXPR: {
      llvm_unreachable("not implemented");
    }
  }
  llvm_unreachable("invalid item kind");
}

// -----------------------------------------------------------------------------
bool Object::Store(uint64_t offset, Ref<Value> value, Type type)
{
  auto it = GetItem(this, offset);
  if (!it) {
    return false;
  }

  switch (value->GetKind()) {
    case Value::Kind::INST: {
      llvm_unreachable("not a constant");
    }
    case Value::Kind::GLOBAL: {
      auto *g = &*::cast<Global>(value);
      return StoreExpr(it->first, it->second, SymbolOffsetExpr::Create(g, 0));
    }
    case Value::Kind::EXPR: {
      return StoreExpr(it->first, it->second, &*::cast<Expr>(value));
    }
    case Value::Kind::CONST: {
      switch (::cast<Constant>(value)->GetKind()) {
        case Constant::Kind::INT: {
          const auto &intValue = ::cast<ConstantInt>(value)->GetValue();
          return StoreInt(it->first, it->second, type, intValue);
        }
        case Constant::Kind::FLOAT: {
          llvm_unreachable("not implemented");
        }
      }
      llvm_unreachable("not a constant");
    }
  }
  llvm_unreachable("invalid value kind");
}



// -----------------------------------------------------------------------------
void llvm::ilist_traits<Object>::deleteNode(Object *object)
{
  delete object;
}

// -----------------------------------------------------------------------------
void llvm::ilist_traits<Object>::addNodeToList(Object *object)
{
  assert(!object->getParent() && "node already in list");
  Data *data = getParent();
  object->setParent(data);
  if (Prog *parent = data->getParent()) {
    for (Atom &atom : *object) {
      parent->insertGlobal(&atom);
    }
  }
}

// -----------------------------------------------------------------------------
void llvm::ilist_traits<Object>::removeNodeFromList(Object *object)
{
  Data *data = getParent();
  object->setParent(nullptr);

  if (Prog *parent = data->getParent()) {
    for (Atom &atom : *object) {
      parent->removeGlobalName(atom.GetName());
    }
  }
}

// -----------------------------------------------------------------------------
void llvm::ilist_traits<Object>::transferNodesFromList(
    ilist_traits &from,
    instr_iterator first,
    instr_iterator last)
{
  Data *data = getParent();
  for (auto it = first; it != last; ++it) {
    llvm_unreachable("not implemented");
  }
}

// -----------------------------------------------------------------------------
Data *llvm::ilist_traits<Object>::getParent()
{
  auto sublist = Data::getSublistAccess(static_cast<Object *>(nullptr));
  size_t offset(size_t(&(static_cast<Data *>(nullptr)->*sublist)));
  Data::ObjectListType *anchor(static_cast<Data::ObjectListType *>(this));
  return reinterpret_cast<Data *>(reinterpret_cast<char*>(anchor) - offset);
}
