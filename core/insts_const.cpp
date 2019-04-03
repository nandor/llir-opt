// This file if part of the genm-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include "core/insts.h"



// -----------------------------------------------------------------------------
ArgInst::ArgInst(Type type, ConstantInt *index, const AnnotSet &annot)
  : ConstInst(Kind::ARG, type, 1, annot)
{
  Op<0>() = index;
}

// -----------------------------------------------------------------------------
unsigned ArgInst::GetIdx() const
{
  return static_cast<ConstantInt *>(Op<0>().get())->GetValue();
}

// -----------------------------------------------------------------------------
FrameInst::FrameInst(Type type, ConstantInt *index, const AnnotSet &annot)
  : ConstInst(Kind::FRAME, type, 1, annot)
{
  Op<0>() = index;
}

// -----------------------------------------------------------------------------
unsigned FrameInst::GetIdx() const
{
  return static_cast<ConstantInt *>(Op<0>().get())->GetValue();
}

// -----------------------------------------------------------------------------
UndefInst::UndefInst(Type type, const AnnotSet &annot)
  : ConstInst(Kind::UNDEF, type, 0, annot)
{
}
