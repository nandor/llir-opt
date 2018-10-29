// This file if part of the genm-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include "core/insts.h"



// -----------------------------------------------------------------------------
unsigned CallInst::getNumOps() const
{
  return 1 + args_.size();
}

// -----------------------------------------------------------------------------
const Operand &CallInst::getOp(unsigned i) const
{
  if (i == 0) return callee_;
  if (i <= args_.size()) return args_[i - 1];
  throw InvalidOperandException();
}

// -----------------------------------------------------------------------------
unsigned TailCallInst::getNumOps() const
{
  return 1 + args_.size();
}

// -----------------------------------------------------------------------------
const Operand &TailCallInst::getOp(unsigned i) const
{
  if (i == 0) return callee_;
  if (i <= args_.size()) return args_[i - 1];
  throw InvalidOperandException();
}

// -----------------------------------------------------------------------------
unsigned ReturnInst::getNumOps() const
{
  return op_ ? 1 : 0;
}

// -----------------------------------------------------------------------------
const Operand &ReturnInst::getOp(unsigned i) const
{
  if (i == 0 && op_) return *op_;
  throw InvalidOperandException();
}

// -----------------------------------------------------------------------------
unsigned JumpTrueInst::getNumOps() const
{
  return 2;
}

// -----------------------------------------------------------------------------
const Operand &JumpTrueInst::getOp(unsigned i) const
{
  if (i == 0) return cond_;
  if (i == 1) return target_;
  throw InvalidOperandException();
}

// -----------------------------------------------------------------------------
unsigned JumpFalseInst::getNumOps() const
{
  return 2;
}

// -----------------------------------------------------------------------------
const Operand &JumpFalseInst::getOp(unsigned i) const
{
  if (i == 0) return cond_;
  if (i == 1) return target_;
  throw InvalidOperandException();
}

// -----------------------------------------------------------------------------
unsigned JumpIndirectInst::getNumOps() const
{
  return 1;
}

// -----------------------------------------------------------------------------
const Operand &JumpIndirectInst::getOp(unsigned i) const
{
  if (i == 0) return target_;
  throw InvalidOperandException();
}

// -----------------------------------------------------------------------------
unsigned JumpInst::getNumOps() const
{
  return 1;
}

// -----------------------------------------------------------------------------
const Operand &JumpInst::getOp(unsigned i) const
{
  if (i == 0) return target_;
  throw InvalidOperandException();
}

// -----------------------------------------------------------------------------
unsigned SelectInst::getNumOps() const
{
  return 3;
}

// -----------------------------------------------------------------------------
const Operand &SelectInst::getOp(unsigned i) const
{
  if (i == 0) return cond_;
  if (i == 1) return vt_;
  if (i == 2) return vf_;
  throw InvalidOperandException();
}

// -----------------------------------------------------------------------------
unsigned SwitchInst::getNumOps() const
{
  return 1 + branches_.size();
}

// -----------------------------------------------------------------------------
const Operand &SwitchInst::getOp(unsigned i) const
{
  if (i == 0) return index_;
  if (i <= branches_.size()) return branches_[i - 1];
  throw InvalidOperandException();
}

// -----------------------------------------------------------------------------
unsigned ExchangeInst::getNumOps() const
{
  return 2;
}

// -----------------------------------------------------------------------------
const Operand &ExchangeInst::getOp(unsigned i) const
{
  if (i == 0) return addr_;
  if (i == 1) return val_;
  throw InvalidOperandException();
}

// -----------------------------------------------------------------------------
unsigned ImmediateInst::getNumOps() const
{
  return 1;
}

// -----------------------------------------------------------------------------
const Operand &ImmediateInst::getOp(unsigned i) const
{
  if (i == 0) return imm_;
  throw InvalidOperandException();
}


// -----------------------------------------------------------------------------
unsigned ArgInst::getNumOps() const
{
  return 1;
}

// -----------------------------------------------------------------------------
const Operand &ArgInst::getOp(unsigned i) const
{
  if (i == 0) return index_;
  throw InvalidOperandException();
}

// -----------------------------------------------------------------------------
unsigned PopInst::getNumOps() const
{
  return 0;
}

// -----------------------------------------------------------------------------
const Operand &PopInst::getOp(unsigned i) const
{
  throw InvalidOperandException();
}

// -----------------------------------------------------------------------------
unsigned AddrInst::getNumOps() const
{
  return 1;
}

// -----------------------------------------------------------------------------
const Operand &AddrInst::getOp(unsigned i) const
{
  if (i == 0) return addr_;
  throw InvalidOperandException();
}

// -----------------------------------------------------------------------------
unsigned LoadInst::getNumOps() const
{
  return 1;
}

// -----------------------------------------------------------------------------
const Operand &LoadInst::getOp(unsigned i) const
{
  if (i == 0) return addr_;
  throw InvalidOperandException();
}

// -----------------------------------------------------------------------------
unsigned PushInst::getNumOps() const
{
  return 1;
}

// -----------------------------------------------------------------------------
const Operand &PushInst::getOp(unsigned i) const
{
  if (i == 0) return val_;
  throw InvalidOperandException();
}

// -----------------------------------------------------------------------------
unsigned StoreInst::getNumOps() const
{
  return 2;
}

// -----------------------------------------------------------------------------
const Operand &StoreInst::getOp(unsigned i) const
{
  if (i == 0) return addr_;
  if (i == 1) return val_;
  throw InvalidOperandException();
}

