// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include <cassert>

#include "core/value.h"



// -----------------------------------------------------------------------------
Value::~Value()
{
  replaceAllUsesWith(nullptr);
}

// -----------------------------------------------------------------------------
bool Value::IsConstant() const
{
  switch (GetKind()) {
    case Value::Kind::INST: {
      return false;
    }
    case Value::Kind::EXPR:
    case Value::Kind::GLOBAL:
    case Value::Kind::CONST: {
      return true;
    }
  }
  llvm_unreachable("invalid value kind");
}

// -----------------------------------------------------------------------------
void Value::replaceAllUsesWith(Value *v)
{
  auto it = use_begin();
  while (it != use_end()) {
    Use &use = *it;
    ++it;
    assert((v == nullptr || (*use).Index() == 0) && "invalid use index");
    use = v;
  }
}

// -----------------------------------------------------------------------------
void Value::replaceAllUsesWith(Ref<Value> v)
{
  auto it = use_begin();
  while (it != use_end()) {
    Use &use = *it;
    ++it;
    use = v;
  }
}

// -----------------------------------------------------------------------------
llvm::iterator_range<Value::use_iterator> Value::uses()
{
  return llvm::make_range(use_begin(), use_end());
}

// -----------------------------------------------------------------------------
llvm::iterator_range<Value::const_use_iterator> Value::uses() const
{
  return llvm::make_range(use_begin(), use_end());
}

// -----------------------------------------------------------------------------
llvm::iterator_range<Value::user_iterator> Value::users()
{
  return llvm::make_range(user_begin(), user_end());
}

// -----------------------------------------------------------------------------
llvm::iterator_range<Value::const_user_iterator> Value::users() const
{
  return llvm::make_range(user_begin(), user_end());
}
