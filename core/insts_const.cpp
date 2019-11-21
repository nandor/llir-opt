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
  return static_cast<ConstantInt *>(Op<0>().get())->GetInt();
}

// -----------------------------------------------------------------------------
FrameInst::FrameInst(
    Type type,
    ConstantInt *object,
    ConstantInt *index,
    const AnnotSet &annot)
  : ConstInst(Kind::FRAME, type, 2, annot)
{
  Op<0>() = object;
  Op<1>() = index;
}

// -----------------------------------------------------------------------------
unsigned FrameInst::GetObject() const
{
  return static_cast<ConstantInt *>(Op<0>().get())->GetInt();
}

// -----------------------------------------------------------------------------
unsigned FrameInst::GetIndex() const
{
  return static_cast<ConstantInt *>(Op<1>().get())->GetInt();
}

// -----------------------------------------------------------------------------
UndefInst::UndefInst(Type type, const AnnotSet &annot)
  : ConstInst(Kind::UNDEF, type, 0, annot)
{
}
