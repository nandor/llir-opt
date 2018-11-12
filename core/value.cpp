// This file if part of the genm-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include "core/value.h"



// -----------------------------------------------------------------------------
Value::~Value()
{
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
