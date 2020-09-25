// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include "core/insts.h"



// -----------------------------------------------------------------------------
ArgInst::ArgInst(Type type, ConstantInt *index, AnnotSet &&annot)
  : ConstInst(Kind::ARG, type, 1, std::move(annot))
{
  Op<0>() = index;
}

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
    AnnotSet &&annot)
  : ConstInst(Kind::FRAME, type, 2, std::move(annot))
{
  Op<0>() = object;
  Op<1>() = index;
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
unsigned FrameInst::GetOffset() const
{
  return static_cast<ConstantInt *>(Op<1>().get())->GetInt();
}

// -----------------------------------------------------------------------------
UndefInst::UndefInst(Type type, AnnotSet &&annot)
  : ConstInst(Kind::UNDEF, type, 0, std::move(annot))
{
}

// -----------------------------------------------------------------------------
UndefInst::UndefInst(Type type, const AnnotSet &annot)
  : ConstInst(Kind::UNDEF, type, 0, annot)
{
}

// -----------------------------------------------------------------------------
MovInst::MovInst(Type type, Value *op, AnnotSet &&annot)
  : OperatorInst(Kind::MOV, type, 1, std::move(annot))
{
  Op<0>() = op;
}

// -----------------------------------------------------------------------------
MovInst::MovInst(Type type, Value *op, const AnnotSet &annot)
  : OperatorInst(Kind::MOV, type, 1, annot)
{
  Op<0>() = op;
}
