// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include "core/insts/memory.h"



// -----------------------------------------------------------------------------
LoadInst::LoadInst(Type type, Value *addr, AnnotSet &&annot)
  : MemoryInst(Kind::LD, 1, std::move(annot))
  , type_(type)
{
  Op<0>() = addr;
}

// -----------------------------------------------------------------------------
unsigned LoadInst::GetNumRets() const
{
  return 1;
}

// -----------------------------------------------------------------------------
Type LoadInst::GetType(unsigned i) const
{
  if (i == 0) return type_;
  llvm_unreachable("invalid operand");
}

// -----------------------------------------------------------------------------
Inst *LoadInst::GetAddr() const
{
  return static_cast<Inst *>(Op<0>().get());
}

// -----------------------------------------------------------------------------
StoreInst::StoreInst(
    Inst *addr,
    Inst *val,
    AnnotSet &&annot)
  : MemoryInst(Kind::ST, 2, std::move(annot))
{
  Op<0>() = addr;
  Op<1>() = val;
}

// -----------------------------------------------------------------------------
unsigned StoreInst::GetNumRets() const
{
  return 0;
}

// -----------------------------------------------------------------------------
Type StoreInst::GetType(unsigned i) const
{
  llvm_unreachable("invalid operand");
}

// -----------------------------------------------------------------------------
Inst *StoreInst::GetAddr() const
{
  return static_cast<Inst *>(Op<0>().get());
}

// -----------------------------------------------------------------------------
Inst *StoreInst::GetVal() const
{
  return static_cast<Inst *>(Op<1>().get());
}

// -----------------------------------------------------------------------------
VAStartInst::VAStartInst(Inst *vaList, AnnotSet &&annot)
  : Inst(Kind::VASTART, 1, std::move(annot))
{
  Op<0>() = vaList;
}

// -----------------------------------------------------------------------------
unsigned VAStartInst::GetNumRets() const
{
  return 0;
}

// -----------------------------------------------------------------------------
Type VAStartInst::GetType(unsigned i) const
{
  llvm_unreachable("invalid operand");
}

// -----------------------------------------------------------------------------
AllocaInst::AllocaInst(
    Type type,
    Inst *size,
    ConstantInt *align,
    AnnotSet &&annot)
  : OperatorInst(Kind::ALLOCA, type, 2, std::move(annot))
{
  Op<0>() = size;
  Op<1>() = align;
}
