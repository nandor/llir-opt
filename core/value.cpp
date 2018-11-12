// This file if part of the genm-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include "core/value.h"



// -----------------------------------------------------------------------------
llvm::iterator_range<use_iterator> Value::uses()
{
  return make_range(use_begin(), use_end());
}

// -----------------------------------------------------------------------------
llvm::iterator_range<const_use_iterator> Value::uses() const
{
  return make_range(use_begin(), use_end());
}

// -----------------------------------------------------------------------------
llvm::iterator_range<user_iterator> Value::users()
{
  return llvm::make_range(user_begin(), user_end());
}

// -----------------------------------------------------------------------------
llvm::iterator_range<const_user_iterator> Value::users() const
{
  return llvm::make_range(user_begin(), user_end());
}
