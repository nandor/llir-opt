// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include "core/global.h"
#include "core/prog.h"


// -----------------------------------------------------------------------------
Global::Global(
    Kind kind,
    const std::string_view name,
    Visibility visibility,
    unsigned numOps)
  : User(Value::Kind::GLOBAL, numOps)
  , kind_(kind)
  , name_(name)
  , visibility_(visibility)
{
}

// -----------------------------------------------------------------------------
Global::~Global()
{
}

// -----------------------------------------------------------------------------
bool Global::IsRoot() const
{
  switch (visibility_) {
    case Visibility::LOCAL:
    case Visibility::GLOBAL_HIDDEN:
    case Visibility::WEAK_HIDDEN: {
      return false;
    }
    case Visibility::GLOBAL_DEFAULT:
    case Visibility::WEAK_DEFAULT:{
      return true;
    }
  }
  llvm_unreachable("invalid extern kind");
}

// -----------------------------------------------------------------------------
bool Global::IsLocal() const
{
  switch (visibility_) {
    case Visibility::LOCAL: {
      return true;
    }
    case Visibility::GLOBAL_DEFAULT:
    case Visibility::GLOBAL_HIDDEN:
    case Visibility::WEAK_DEFAULT:
    case Visibility::WEAK_HIDDEN: {
      return false;
    }
  }
  llvm_unreachable("invalid extern kind");
}

// -----------------------------------------------------------------------------
bool Global::IsWeak() const
{
  switch (visibility_) {
    case Visibility::LOCAL:
    case Visibility::GLOBAL_DEFAULT:
    case Visibility::GLOBAL_HIDDEN: {
      return false;
    }
    case Visibility::WEAK_DEFAULT:
    case Visibility::WEAK_HIDDEN: {
      return true;
    }
  }
  llvm_unreachable("invalid extern kind");
}
