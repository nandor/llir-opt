// This file if part of the genm-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include "core/block.h"
#include "core/context.h"
#include "core/insts.h"
#include "core/symbol.h"



// -----------------------------------------------------------------------------
SelectInst::SelectInst(Block *block, Type type, Inst *cond, Inst *vt, Inst *vf)
  : OperatorInst(Kind::SELECT, block, type, 3)
{
  Op<0>() = cond;
  Op<1>() = vt;
  Op<2>() = vf;
}

// -----------------------------------------------------------------------------
SetInst::SetInst(Block *block, Value *reg, Inst *val)
  : Inst(Kind::SET, block, 2)
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
  throw InvalidOperandException();
}

// -----------------------------------------------------------------------------
ImmInst::ImmInst(Block *block, Type type, Constant *imm)
  : ConstInst(Kind::IMM, block, type, 1)
{
  Op<0>() = imm;
}

// -----------------------------------------------------------------------------
union ImmValue {
  float f32v;
  double f64v;
  int8_t i8v;
  int16_t i16v;
  int32_t i32v;
  int64_t i64v;
};

// -----------------------------------------------------------------------------
ImmValue GetValue(Constant *cst)
{
  ImmValue val;
  switch (cst->GetKind()) {
    case Constant::Kind::INT: {
      val.i64v = static_cast<ConstantInt *>(cst)->GetValue();
      break;
    }
    case Constant::Kind::FLOAT: {
      val.f64v = static_cast<ConstantFloat *>(cst)->GetValue();
      break;
    }
    case Constant::Kind::REG: {
      assert(!"invalid constant");
      break;
    }
    case Constant::Kind::UNDEF: {
      assert(!"invalid constant");
      break;
    }
  }
  return val;
}

// -----------------------------------------------------------------------------
int64_t ImmInst::GetI8() const
{
  return GetValue(static_cast<Constant *>(Op<0>().get())).i8v;
}

// -----------------------------------------------------------------------------
int64_t ImmInst::GetI16() const
{
  return GetValue(static_cast<Constant *>(Op<0>().get())).i16v;
}

// -----------------------------------------------------------------------------
int64_t ImmInst::GetI32() const
{
  return GetValue(static_cast<Constant *>(Op<0>().get())).i32v;
}

// -----------------------------------------------------------------------------
int64_t ImmInst::GetI64() const
{
  return GetValue(static_cast<Constant *>(Op<0>().get())).i64v;
}

// -----------------------------------------------------------------------------
double ImmInst::GetF32() const
{
  return GetValue(static_cast<Constant *>(Op<0>().get())).f32v;
}

// -----------------------------------------------------------------------------
double ImmInst::GetF64() const
{
  return GetValue(static_cast<Constant *>(Op<0>().get())).f64v;
}

// -----------------------------------------------------------------------------
ArgInst::ArgInst(Block *block, Type type, ConstantInt *index)
  : ConstInst(Kind::ARG, block, type, 1)
{
  Op<0>() = index;
}

// -----------------------------------------------------------------------------
unsigned ArgInst::GetIdx() const
{
  return static_cast<ConstantInt *>(Op<0>().get())->GetValue();
}

// -----------------------------------------------------------------------------
AddrInst::AddrInst(Block *block, Type type, Value *addr)
  : ConstInst(Kind::ADDR, block, type, 1)
{
  Op<0>() = addr;
}

// -----------------------------------------------------------------------------
Value *AddrInst::GetAddr() const
{
  return Op<0>();
}

// -----------------------------------------------------------------------------
FrameInst::FrameInst(Block *block, Type type, ConstantInt *index)
  : OperatorInst(Kind::FRAME, block, type, 1)
{
  Op<0>() = index;
}

// -----------------------------------------------------------------------------
unsigned FrameInst::GetIdx() const
{
  return static_cast<ConstantInt *>(Op<0>().get())->GetValue();
}

// -----------------------------------------------------------------------------
PhiInst::PhiInst(Block *block, Type type)
  : Inst(Kind::PHI, block, 0)
  , type_(type)
{
}

// -----------------------------------------------------------------------------
unsigned PhiInst::GetNumRets() const
{
  return 1;
}

// -----------------------------------------------------------------------------
Type PhiInst::GetType(unsigned i) const
{
  if (i == 0) return type_;
  throw InvalidOperandException();
}

// -----------------------------------------------------------------------------
void PhiInst::Add(Block *block, Value *value)
{
  for (unsigned i = 0, n = GetNumIncoming(); i < n; ++i) {
    if (GetBlock(i) == block) {
      *(op_begin() + i * 2 + 1) = value;
      return;
    }
  }
  growUses(numOps_ + 2);
  Op<-2>() = block;
  Op<-1>() = value;
}

// -----------------------------------------------------------------------------
unsigned PhiInst::GetNumIncoming() const
{
  assert((numOps_ & 1) == 0 && "invalid node count");
  return numOps_ >> 1;
}

// -----------------------------------------------------------------------------
Block *PhiInst::GetBlock(unsigned i) const
{
  const Use *use = op_begin() + i * 2 + 0;
  return static_cast<Block *>((op_begin() + i * 2 + 0)->get());
}

// -----------------------------------------------------------------------------
Value *PhiInst::GetValue(unsigned i) const
{
  return static_cast<Block *>((op_begin() + i * 2 + 1)->get());
}

// -----------------------------------------------------------------------------
bool PhiInst::HasValue(const Block *block) const
{
  for (unsigned i = 0; i < GetNumIncoming(); ++i) {
    if (GetBlock(i) == block) {
      return true;
    }
  }
  return false;
}

// -----------------------------------------------------------------------------
Value *PhiInst::GetValue(const Block *block) const
{
  for (unsigned i = 0; i < GetNumIncoming(); ++i) {
    if (GetBlock(i) == block) {
      return GetValue(i);
    }
  }
  throw InvalidPredecessorException();
}
