// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include "core/insts/memory.h"



// -----------------------------------------------------------------------------
LoadInst::LoadInst(
    Type type,
    Value *addr,
    const AnnotSet &annot)
  : MemoryInst(Kind::LD, 1, annot)
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
    const AnnotSet &annot)
  : MemoryInst(Kind::ST, 2, annot)
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
XchgInst::XchgInst(
    Type type,
    Inst *addr,
    Inst *val,
    const AnnotSet &annot)
  : MemoryInst(Kind::XCHG, 2, annot)
  , type_(type)
{
  Op<0>() = addr;
  Op<1>() = val;
}

// -----------------------------------------------------------------------------
unsigned XchgInst::GetNumRets() const
{
  return 1;
}

// -----------------------------------------------------------------------------
Type XchgInst::GetType(unsigned i) const
{
  if (i == 0) return type_;
  llvm_unreachable("invalid operand");
}

// -----------------------------------------------------------------------------
CmpXchgInst::CmpXchgInst(
    Type type,
    Inst *addr,
    Inst *val,
    Inst *ref,
    const AnnotSet &annot)
  : MemoryInst(Kind::CMPXCHG, 3, annot)
  , type_(type)
{
  Op<0>() = addr;
  Op<1>() = val;
  Op<2>() = ref;
}

// -----------------------------------------------------------------------------
unsigned CmpXchgInst::GetNumRets() const
{
  return 1;
}

// -----------------------------------------------------------------------------
Type CmpXchgInst::GetType(unsigned i) const
{
  if (i == 0) return type_;
  llvm_unreachable("invalid operand");
}

// -----------------------------------------------------------------------------
VAStartInst::VAStartInst(Inst *vaList, const AnnotSet &annot)
  : Inst(Kind::VASTART, 1, annot)
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
    const AnnotSet &annot)
  : OperatorInst(Kind::ALLOCA, type, 2, annot)
{
  Op<0>() = size;
  Op<1>() = align;
}
