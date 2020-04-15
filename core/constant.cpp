// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include "core/constant.h"



// -----------------------------------------------------------------------------
Constant::~Constant()
{
  llvm_unreachable("should never be deleted");
}

// -----------------------------------------------------------------------------
double ConstantFloat::GetDouble() const
{
  APFloat r(v_);
  bool i;
  r.convert(llvm::APFloat::IEEEdouble(), APFloat::rmNearestTiesToEven, &i);
  return r.convertToDouble();
}
