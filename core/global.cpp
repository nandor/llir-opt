// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include "core/global.h"
#include "core/prog.h"



// -----------------------------------------------------------------------------
Global::~Global()
{
}

// -----------------------------------------------------------------------------
bool Global::IsExtern() const
{
  switch (visibility_) {
    case Visibility::WEAK_EXTERN:
    case Visibility::EXTERN: {
      return true;
    }
    case Visibility::DEFAULT:
    case Visibility::WEAK_DEFAULT:
    case Visibility::HIDDEN:
    case Visibility::WEAK_HIDDEN: {
      return false;
    }
  }
  llvm_unreachable("invalid extern kind");
}

// -----------------------------------------------------------------------------
bool Global::IsHidden() const
{
  switch (visibility_) {
    case Visibility::WEAK_EXTERN:
    case Visibility::EXTERN: {
      return false;
    }
    case Visibility::DEFAULT:
    case Visibility::WEAK_DEFAULT:
    case Visibility::HIDDEN:
    case Visibility::WEAK_HIDDEN: {
      return true;
    }
  }
  llvm_unreachable("invalid extern kind");
}

// -----------------------------------------------------------------------------
bool Global::IsWeak() const
{
  switch (visibility_) {
    case Visibility::DEFAULT:
    case Visibility::EXTERN:
    case Visibility::HIDDEN: {
      return false;
    }
    case Visibility::WEAK_DEFAULT:
    case Visibility::WEAK_EXTERN:
    case Visibility::WEAK_HIDDEN: {
      return true;
    }
  }
  llvm_unreachable("invalid extern kind");
}
