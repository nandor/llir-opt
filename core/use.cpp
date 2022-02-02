// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include "core/use.h"

#include <cassert>
#include <cstdint>

#include "core/value.h"



// -----------------------------------------------------------------------------
Use::Use(Ref<Value> val, User *user)
  : val_(val), user_(user)
{
  Add();
}

// -----------------------------------------------------------------------------
Use &Use::operator=(Ref<Value> val)
{
  Remove();
  val_ = val;
  Add();
  return *this;
}

// -----------------------------------------------------------------------------
void Use::Remove()
{
  if (val_ && (reinterpret_cast<uintptr_t>(val_.Get()) & 1) == 0) {
    if (next_) { next_->prev_ = prev_; }
    if (prev_) { prev_->next_ = next_; }
    if (this == val_->users_) { val_->users_ = next_; }
    next_ = prev_ = nullptr;
  }
}

// -----------------------------------------------------------------------------
void Use::Add()
{
  if (val_ && (reinterpret_cast<uintptr_t>(val_.Get()) & 1) == 0) {
    next_ = val_->users_;
    prev_ = nullptr;
    if (next_) { next_->prev_ = this; }
    val_->users_ = this;
  }
}

// -----------------------------------------------------------------------------
Use::~Use()
{
  Remove();
}
