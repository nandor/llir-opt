// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include "core/inst.h"
#include "core/insts/hardware.h"



// -----------------------------------------------------------------------------
RdtscInst::RdtscInst(Type type, const AnnotSet &annot)
  : OperatorInst(Inst::Kind::RDTSC, type, 0, annot)
{
}

// -----------------------------------------------------------------------------
SetInst::SetInst(ConstantReg *reg, Inst *val, const AnnotSet &annot)
  : Inst(Kind::SET, 2, annot)
{
  Op<0>() = reg;
  Op<1>() = val;
}

// -----------------------------------------------------------------------------
unsigned SetInst::GetNumRets() const
{
  return 0;
}

// -----------------------------------------------------------------------------
Type SetInst::GetType(unsigned i) const
{
  llvm_unreachable("invalid operand");
}

// -----------------------------------------------------------------------------
FNStCwInst::FNStCwInst(Inst *addr, const AnnotSet &annot)
  : Inst(Kind::FNSTCW, 1, annot)
{
  Op<0>() = addr;
}

// -----------------------------------------------------------------------------
unsigned FNStCwInst::GetNumRets() const
{
  return 0;
}

// -----------------------------------------------------------------------------
Type FNStCwInst::GetType(unsigned i) const
{
  llvm_unreachable("invalid operand");
}

// -----------------------------------------------------------------------------
FLdCwInst::FLdCwInst(Inst *addr, const AnnotSet &annot)
  : Inst(Kind::FNSTCW, 1, annot)
{
  Op<0>() = addr;
}

// -----------------------------------------------------------------------------
unsigned FLdCwInst::GetNumRets() const
{
  return 0;
}

// -----------------------------------------------------------------------------
Type FLdCwInst::GetType(unsigned i) const
{
  llvm_unreachable("invalid operand");
}
