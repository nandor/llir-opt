// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include <cassert>

#include "core/value.h"


// -----------------------------------------------------------------------------
Use &Use::operator = (Value *val)
{
  Remove();
  val_ = val;
  Add();
  return *this;
}

// -----------------------------------------------------------------------------
void Use::Remove()
{
  if (val_ && (reinterpret_cast<uintptr_t>(val_) & 1) == 0) {
    if (next_) { next_->prev_ = prev_; }
    if (prev_) { prev_->next_ = next_; }
    if (this == val_->users_) { val_->users_ = next_; }
    next_ = prev_ = nullptr;
  }
}

// -----------------------------------------------------------------------------
void Use::Add()
{
  if (val_ && (reinterpret_cast<uintptr_t>(val_) & 1) == 0) {
    next_ = val_->users_;
    prev_ = nullptr;
    if (next_) { next_->prev_ = this; }
    val_->users_ = this;
  }
}

// -----------------------------------------------------------------------------
Use::~Use()
{
  assert(!"not implemented");
}



// -----------------------------------------------------------------------------
Value::~Value()
{
  replaceAllUsesWith(nullptr);
}

// -----------------------------------------------------------------------------
void Value::replaceAllUsesWith(Value *v)
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
        (new (&newUses[i]) Use(uses_[i].val_, this))->Add();
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

