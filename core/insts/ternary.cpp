// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include "core/insts/ternary.h"



// -----------------------------------------------------------------------------
SelectInst::SelectInst(
    Type type,
    Inst *cond,
    Inst *vt,
    Inst *vf,
    AnnotSet &&annot)
  : OperatorInst(Kind::SELECT, type, 3, std::move(annot))
{
  Op<0>() = cond;
  Op<1>() = vt;
  Op<2>() = vf;
}

// -----------------------------------------------------------------------------
SelectInst::SelectInst(
    Type type,
    Inst *cond,
    Inst *vt,
    Inst *vf,
    const AnnotSet &annot)
  : OperatorInst(Kind::SELECT, type, 3, annot)
{
  Op<0>() = cond;
  Op<1>() = vt;
  Op<2>() = vf;
}
