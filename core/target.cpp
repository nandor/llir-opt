// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include "core/target.h"



// -----------------------------------------------------------------------------
Type Target::GetPointerType() const
{
  return triple_.isArch32Bit() ? Type::I32 : Type::I64;
}
