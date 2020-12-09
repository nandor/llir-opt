// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include "core/visibility.h"



// -----------------------------------------------------------------------------
llvm::raw_ostream &operator<<(llvm::raw_ostream &os, Visibility visibility)
{
  switch (visibility) {
    case Visibility::LOCAL:           return os << "local";
    case Visibility::GLOBAL_DEFAULT:  return os << "global_default";
    case Visibility::GLOBAL_HIDDEN:   return os << "global_hidden";
    case Visibility::WEAK_DEFAULT:    return os << "weak_default";
    case Visibility::WEAK_HIDDEN:     return os << "weak_hidden";
  }
  llvm_unreachable("invalid visibility");
}
