// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include "core/constant.h"



// -----------------------------------------------------------------------------
Constant::~Constant()
{
}

// -----------------------------------------------------------------------------
ConstantInt::ConstantInt(int64_t v)
  : Constant(Constant::Kind::INT)
  , v_(64, v, true)
{
}

// -----------------------------------------------------------------------------
double ConstantFloat::GetDouble() const
{
  APFloat r(v_);
  bool i;
  r.convert(llvm::APFloat::IEEEdouble(), APFloat::rmNearestTiesToEven, &i);
  return r.convertToDouble();
}
