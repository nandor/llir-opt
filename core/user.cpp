// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include <cassert>

#include "core/user.h"
#include "core/block.h"
#include "core/cast.h"



// -----------------------------------------------------------------------------
template<>
Ref<Inst> User::conv_op_iterator<Inst>::operator*() const
{
  return ::cast<Inst>(*this->I);
}

// -----------------------------------------------------------------------------
template<>
Ref<Inst> User::conv_op_iterator<Inst>::operator->() const
{
  return ::cast<Inst>(*this->I);
}

// -----------------------------------------------------------------------------
template<>
ConstRef<Inst> User::const_conv_op_iterator<Inst>::operator*() const
{
  return ::cast<Inst>(*this->I);
}

// -----------------------------------------------------------------------------
template<>
ConstRef<Inst> User::const_conv_op_iterator<Inst>::operator->() const
{
  return ::cast<Inst>(*this->I);
}

// -----------------------------------------------------------------------------
template<>
Ref<Block> User::conv_op_iterator<Block>::operator*() const
{
  return ::cast<Block>(*this->I);
}

// -----------------------------------------------------------------------------
template<>
Ref<Block> User::conv_op_iterator<Block>::operator->() const
{
  return ::cast<Block>(*this->I);
}

// -----------------------------------------------------------------------------
template<>
ConstRef<Block> User::const_conv_op_iterator<Block>::operator*() const
{
  return ::cast<Block>(*this->I);
}

// -----------------------------------------------------------------------------
template<>
ConstRef<Block> User::const_conv_op_iterator<Block>::operator->() const
{
  return ::cast<Block>(*this->I);
}

// -----------------------------------------------------------------------------
User::User(Kind kind, unsigned numOps)
  : Value(kind)
  , numOps_(numOps)
  , uses_(nullptr)
{
  if (numOps > 0) {
    uses_ = static_cast<Use *>(malloc(numOps_ * sizeof(Use)));
    for (unsigned i = 0; i < numOps_; ++i) {
      new (&uses_[i]) Use(nullptr, this);
    }
  }
}

// -----------------------------------------------------------------------------
User::~User()
{
  for (unsigned i = 0; i < numOps_; ++i) {
    uses_[i] = nullptr;
  }
  free(static_cast<void *>(uses_));
}

// -----------------------------------------------------------------------------
User::op_range User::operands()
{
  return op_range(op_begin(), op_end());
}

// -----------------------------------------------------------------------------
User::const_op_range User::operands() const
{
  return const_op_range(op_begin(), op_end());
}

// -----------------------------------------------------------------------------
User::value_op_iterator User::value_op_begin()
{
  return value_op_iterator(op_begin());
}

// -----------------------------------------------------------------------------
User::value_op_iterator User::value_op_end()
{
  return value_op_iterator(op_end());
}

// -----------------------------------------------------------------------------
User::value_op_range User::operand_values()
{
  return llvm::make_range(value_op_begin(), value_op_end());
}

// -----------------------------------------------------------------------------
User::const_value_op_iterator User::value_op_begin() const
{
  return const_value_op_iterator(op_begin());
}

// -----------------------------------------------------------------------------
User::const_value_op_iterator User::value_op_end() const
{
  return const_value_op_iterator(op_end());
}

// -----------------------------------------------------------------------------
User::const_value_op_range User::operand_values() const
{
  return llvm::make_range(value_op_begin(), value_op_end());
}

// -----------------------------------------------------------------------------
void User::resizeUses(unsigned n)
{
  if (n == 0) {
    // Delete the use lists.
    for (unsigned i = 0; i < numOps_; ++i) {
      uses_[i] = nullptr;
    }
    free(static_cast<void *>(uses_));
    uses_ = nullptr;
    numOps_ = n;
  } else {
    // Transfer old uses to newly allocated ones.
    Use *newUses = static_cast<Use *>(malloc(n * sizeof(Use)));
    for (unsigned i = 0; i < numOps_; ++i) {
      uses_[i].Remove();
      if (i < n) {
        new (&newUses[i]) Use(uses_[i].val_, this);
      }
    }

    // Switch the lists.
    if (uses_) {
      free(uses_);
    }
    uses_ = newUses;

    // Initialise the new elements.
    for (unsigned i = numOps_; i < n; ++i) {
      new (&uses_[i]) Use(nullptr, this);
    }
    numOps_ = n;
  }
}

