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
      if (size == 1 && off == 0) {
        return new ConstantInt(it->GetInt8());
      }
      return nullptr;
    }
    case Item::Kind::INT16: {
      if (size == 2 && off == 0) {
        return new ConstantInt(it->GetInt16());
      }
      return nullptr;
    }
    case Item::Kind::INT32: {
      if (size == 4 && off == 0) {
        return new ConstantInt(it->GetInt32());
      }
      return nullptr;
    }
    case Item::Kind::INT64: {
      if (size == 8 && off == 0) {
        return new ConstantInt(it->GetInt64());
      }
      return nullptr;
    }
    case Item::Kind::STRING: {
      if (size == 1 && off == 0) {
        return new ConstantInt(it->getString()[off]);
      }
      return nullptr;
    }
    case Item::Kind::SPACE: {
      if (off + size <= it->GetSpace()) {
        return new ConstantInt(0);
      }
      return nullptr;
    }
    case Item::Kind::FLOAT64: {
      return nullptr;
    }
    case Item::Kind::EXPR32: {
      auto *expr = it->GetExpr();
      switch (expr->GetKind()) {
        case Expr::Kind::SYMBOL_OFFSET: {
          auto *sym = static_cast<SymbolOffsetExpr *>(expr);
          if (size == 4) {
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
    case Item::Kind::EXPR64: {
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
  llvm_unreachable("invalid item kind");
}

// -----------------------------------------------------------------------------
static Value *LoadFloat(
    Atom::iterator it,
    unsigned off,
    const llvm::fltSemantics &sema)
{
  switch (APFloat::SemanticsToEnum(sema)) {
    case APFloat::S_IEEEhalf: {
      llvm_unreachable("not implemented");
    }
    case APFloat::S_BFloat: {
      llvm_unreachable("not implemented");
    }
    case APFloat::S_IEEEsingle: {
      llvm_unreachable("not implemented");
    }
    case APFloat::S_IEEEdouble: {
      if (it->GetKind() == Item::Kind::FLOAT64) {
        return new ConstantFloat(it->GetFloat64());
      }
      return nullptr;
    }
    case APFloat::S_x87DoubleExtended: {
      return nullptr;
    }
    case APFloat::S_IEEEquad: {
      llvm_unreachable("not implemented");
    }
    case APFloat::S_PPCDoubleDouble: {
      llvm_unreachable("not implemented");
    }
  }
  llvm_unreachable("invalid semantics");
}

// -----------------------------------------------------------------------------
Value *Object::Load(uint64_t offset, Type type)
{
  auto it = GetItem(this, offset);
  if (!it) {
    return nullptr;
  }

  switch (type) {
    case Type::I8:
    case Type::I16:
    case Type::I32:
    case Type::I64:
    case Type::V64:
    case Type::I128: {
      return LoadInt(it->first, it->second, GetSize(type));
    }
    case Type::F32: {
      return LoadFloat(it->first, it->second, APFloat::IEEEsingle());
    }
    case Type::F64: {
      return LoadFloat(it->first, it->second, APFloat::IEEEdouble());
    }
    case Type::F128: {
      return LoadFloat(it->first, it->second, APFloat::IEEEquad());
    }
    case Type::F80: {
      return LoadFloat(it->first, it->second, APFloat::x87DoubleExtended());
    }
  }
  llvm_unreachable("invalid type");
}

// -----------------------------------------------------------------------------
static bool StoreExpr(Atom::iterator it, unsigned off, Expr *expr, Type ty)
{
  switch (it->GetKind()) {
    case Item::Kind::INT8:
    case Item::Kind::INT16:
    case Item::Kind::INT32:
    case Item::Kind::INT64:
    case Item::Kind::EXPR32:
    case Item::Kind::EXPR64:
    case Item::Kind::FLOAT64: {
      if (it->GetSize() == GetSize(ty)) {
        auto *item = Item::CreateExpr64(expr);
        it->getParent()->AddItem(item, &*it);
        it->eraseFromParent();
        return true;
      } else {
        return false;
      }
    }
    case Item::Kind::STRING: {
      llvm_unreachable("not implemented");
    }
    case Item::Kind::SPACE: {
      int64_t space = it->GetSpace();
      int64_t before = off;
      int64_t after = space - off - GetSize(ty);
      assert(after >= 0 && "invalid write");
      auto *atom = it->getParent();
      if (before > 0) {
        atom->AddItem(Item::CreateSpace(before), &*it);
      }
      switch (ty) {
        case Type::I8:
        case Type::I16:
        case Type::I128: {
          llvm_unreachable("not implemented");
        }
        case Type::I32: {
          atom->AddItem(Item::CreateExpr32(expr), &*it);
          break;
        }
        case Type::I64:
        case Type::V64: {
          atom->AddItem(Item::CreateExpr64(expr), &*it);
          break;
        }
        case Type::F32:
        case Type::F64:
        case Type::F80:
        case Type::F128: {
          llvm_unreachable("not implemented");
        }
      }
      if (after > 0) {
        atom->AddItem(Item::CreateSpace(after), &*it);
      }
      it->eraseFromParent();
      return true;
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
        auto *item = Item::CreateInt8(value.getSExtValue());
        it->getParent()->AddItem(item, &*it);
        it->eraseFromParent();
        return true;
      }
      llvm_unreachable("not implemented");
    }
    case Item::Kind::INT16: {
      llvm_unreachable("not implemented");
    }
    case Item::Kind::INT32: 
    case Item::Kind::EXPR32: {
      if (type == Type::I32) {
        auto *item = Item::CreateInt32(value.getSExtValue());
        it->getParent()->AddItem(item, &*it);
        it->eraseFromParent();
        return true;
      }
      llvm_unreachable("not implemented");
    }
    case Item::Kind::INT64: 
    case Item::Kind::EXPR64: {
      if (type == Type::I64 || type == Type::V64) {
        auto *item = Item::CreateInt64(value.getSExtValue());
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
      int64_t space = it->GetSpace();
      int64_t before = off;
      int64_t after = space - off - GetSize(type);
      assert(after >= 0 && "invalid write");
      auto *atom = it->getParent();
      if (before > 0) {
        atom->AddItem(Item::CreateSpace(before), &*it);
      }
      switch (type) {
        case Type::I8: {
          atom->AddItem(Item::CreateInt8(value.getSExtValue()), &*it);
          break;
        }
        case Type::I16:{
          atom->AddItem(Item::CreateInt16(value.getSExtValue()), &*it);
          break;
        }
        case Type::I32: {
          atom->AddItem(Item::CreateInt32(value.getSExtValue()), &*it);
          break;
        }
        case Type::I64: case Type::V64: {
          atom->AddItem(Item::CreateInt64(value.getSExtValue()), &*it);
          break;
        }
        case Type::F32:
        case Type::F64:
        case Type::F80:
        case Type::F128: {
          llvm_unreachable("not implemented");
        }
        case Type::I128: {
          llvm_unreachable("not implemented");
        }
      }
      if (after > 0) {
        atom->AddItem(Item::CreateSpace(after), &*it);
      }
      it->eraseFromParent();
      return true;
    }
    case Item::Kind::FLOAT64: {
      llvm_unreachable("not implemented");
    }
  }
  llvm_unreachable("invalid item kind");
}

// -----------------------------------------------------------------------------
bool Object::Store(uint64_t offset, Ref<Value> value, Type ty)
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
      return StoreExpr(it->first, it->second, SymbolOffsetExpr::Create(g, 0), ty);
    }
    case Value::Kind::EXPR: {
      return StoreExpr(it->first, it->second, &*::cast<Expr>(value), ty);
    }
    case Value::Kind::CONST: {
      switch (::cast<Constant>(value)->GetKind()) {
        case Constant::Kind::INT: {
          const auto &intValue = ::cast<ConstantInt>(value)->GetValue();
          return StoreInt(it->first, it->second, ty, intValue);
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
