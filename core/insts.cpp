// This file if part of the genm-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include "core/insts.h"



// -----------------------------------------------------------------------------
unsigned CallInst::GetNumOps() const
{
  return 1 + args_.size();
}

// -----------------------------------------------------------------------------
unsigned CallInst::GetNumRets() const
{
  return 1;
}

// -----------------------------------------------------------------------------
const Operand &CallInst::GetOp(unsigned i) const
{
  if (i == 0) return callee_;
  if (i <= args_.size()) return args_[i - 1];
  throw InvalidOperandException();
}

// -----------------------------------------------------------------------------
void CallInst::SetOp(unsigned i, const Operand &op)
{
  if (i == 0) { callee_ = op; return; }
  if (i <= args_.size()) { args_[i - 1] = op; return; }
  throw InvalidOperandException();
}

// -----------------------------------------------------------------------------
unsigned TailCallInst::GetNumOps() const
{
  return 1 + args_.size();
}

// -----------------------------------------------------------------------------
const Operand &TailCallInst::GetOp(unsigned i) const
{
  if (i == 0) return callee_;
  if (i <= args_.size()) return args_[i - 1];
  throw InvalidOperandException();
}

// -----------------------------------------------------------------------------
void TailCallInst::SetOp(unsigned i, const Operand &op)
{
  if (i == 0) { callee_ = op; return; }
  if (i <= args_.size()) { args_[i - 1] = op; return; }
  throw InvalidOperandException();
}

// -----------------------------------------------------------------------------
unsigned ReturnInst::GetNumOps() const
{
  return op_ ? 1 : 0;
}

// -----------------------------------------------------------------------------
const Operand &ReturnInst::GetOp(unsigned i) const
{
  if (i == 0 && op_) return *op_;
  throw InvalidOperandException();
}

// -----------------------------------------------------------------------------
void ReturnInst::SetOp(unsigned i, const Operand &op)
{
  if (i == 0 && op_) { op_ = op; return; }
  throw InvalidOperandException();
}

// -----------------------------------------------------------------------------
unsigned JumpTrueInst::GetNumOps() const
{
  return 2;
}

// -----------------------------------------------------------------------------
const Operand &JumpTrueInst::GetOp(unsigned i) const
{
  if (i == 0) return cond_;
  if (i == 1) return target_;
  throw InvalidOperandException();
}

// -----------------------------------------------------------------------------
void JumpTrueInst::SetOp(unsigned i, const Operand &op)
{
  if (i == 0) { cond_ = op; return; }
  if (i == 1) { target_ = op; return; }
  throw InvalidOperandException();
}

// -----------------------------------------------------------------------------
unsigned JumpFalseInst::GetNumOps() const
{
  return 2;
}

// -----------------------------------------------------------------------------
const Operand &JumpFalseInst::GetOp(unsigned i) const
{
  if (i == 0) return cond_;
  if (i == 1) return target_;
  throw InvalidOperandException();
}

// -----------------------------------------------------------------------------
void JumpFalseInst::SetOp(unsigned i, const Operand &op)
{
  if (i == 0) { cond_ = op; return; }
  if (i == 1) { target_ = op; return; }
  throw InvalidOperandException();
}

// -----------------------------------------------------------------------------
unsigned JumpIndirectInst::GetNumOps() const
{
  return 1;
}

// -----------------------------------------------------------------------------
const Operand &JumpIndirectInst::GetOp(unsigned i) const
{
  if (i == 0) return target_;
  throw InvalidOperandException();
}

// -----------------------------------------------------------------------------
void JumpIndirectInst::SetOp(unsigned i, const Operand &op)
{
  if (i == 0) { target_ = op; return; }
  throw InvalidOperandException();
}

// -----------------------------------------------------------------------------
unsigned JumpInst::GetNumOps() const
{
  return 1;
}

// -----------------------------------------------------------------------------
const Operand &JumpInst::GetOp(unsigned i) const
{
  if (i == 0) return target_;
  throw InvalidOperandException();
}

// -----------------------------------------------------------------------------
void JumpInst::SetOp(unsigned i, const Operand &op)
{
  if (i == 0) { target_ = op; return; }
  throw InvalidOperandException();
}

// -----------------------------------------------------------------------------
unsigned SelectInst::GetNumOps() const
{
  return 3;
}

// -----------------------------------------------------------------------------
unsigned SelectInst::GetNumRets() const
{
  return 1;
}

// -----------------------------------------------------------------------------
const Operand &SelectInst::GetOp(unsigned i) const
{
  if (i == 0) return cond_;
  if (i == 1) return vt_;
  if (i == 2) return vf_;
  throw InvalidOperandException();
}

// -----------------------------------------------------------------------------
void SelectInst::SetOp(unsigned i, const Operand &op)
{
  if (i == 0) { cond_ = op; return; }
  if (i == 1) { vt_ = op; return; }
  if (i == 2) { vf_ = op; return; }
  throw InvalidOperandException();
}

// -----------------------------------------------------------------------------
unsigned SwitchInst::GetNumOps() const
{
  return 1 + branches_.size();
}

// -----------------------------------------------------------------------------
const Operand &SwitchInst::GetOp(unsigned i) const
{
  if (i == 0) return index_;
  if (i <= branches_.size()) return branches_[i - 1];
  throw InvalidOperandException();
}

// -----------------------------------------------------------------------------
void SwitchInst::SetOp(unsigned i, const Operand &op)
{
  if (i == 0) { index_ = op; return; }
  if (i <= branches_.size()) { branches_[i - 1] = op; return; }
  throw InvalidOperandException();
}

// -----------------------------------------------------------------------------
unsigned ExchangeInst::GetNumOps() const
{
  return 2;
}

// -----------------------------------------------------------------------------
unsigned ExchangeInst::GetNumRets() const
{
  return 1;
}

// -----------------------------------------------------------------------------
const Operand &ExchangeInst::GetOp(unsigned i) const
{
  if (i == 0) return addr_;
  if (i == 1) return val_;
  throw InvalidOperandException();
}

// -----------------------------------------------------------------------------
void ExchangeInst::SetOp(unsigned i, const Operand &op)
{
  if (i == 0) { addr_ = op; return; }
  if (i == 1) { val_ = op; return; }
  throw InvalidOperandException();
}

// -----------------------------------------------------------------------------
unsigned ImmediateInst::GetNumOps() const
{
  return 1;
}

// -----------------------------------------------------------------------------
unsigned ImmediateInst::GetNumRets() const
{
  return 1;
}

// -----------------------------------------------------------------------------
const Operand &ImmediateInst::GetOp(unsigned i) const
{
  if (i == 0) return imm_;
  throw InvalidOperandException();
}

// -----------------------------------------------------------------------------
void ImmediateInst::SetOp(unsigned i, const Operand &op)
{
  if (i == 0) { imm_ = op; return; }
  throw InvalidOperandException();
}

// -----------------------------------------------------------------------------
unsigned ArgInst::GetNumOps() const
{
  return 1;
}

// -----------------------------------------------------------------------------
unsigned ArgInst::GetNumRets() const
{
  return 1;
}

// -----------------------------------------------------------------------------
const Operand &ArgInst::GetOp(unsigned i) const
{
  if (i == 0) return index_;
  throw InvalidOperandException();
}

// -----------------------------------------------------------------------------
void ArgInst::SetOp(unsigned i, const Operand &op)
{
  if (i == 0) { index_ = op; return; }
  throw InvalidOperandException();
}

// -----------------------------------------------------------------------------
unsigned AddrInst::GetNumOps() const
{
  return 1;
}

// -----------------------------------------------------------------------------
unsigned AddrInst::GetNumRets() const
{
  return 1;
}

// -----------------------------------------------------------------------------
const Operand &AddrInst::GetOp(unsigned i) const
{
  if (i == 0) return addr_;
  throw InvalidOperandException();
}

// -----------------------------------------------------------------------------
void AddrInst::SetOp(unsigned i, const Operand &op)
{
  if (i == 0) { addr_ = op; return; }
  throw InvalidOperandException();
}

// -----------------------------------------------------------------------------
unsigned LoadInst::GetNumOps() const
{
  return 1;
}

// -----------------------------------------------------------------------------
unsigned LoadInst::GetNumRets() const
{
  return 1;
}

// -----------------------------------------------------------------------------
const Operand &LoadInst::GetOp(unsigned i) const
{
  if (i == 0) return addr_;
  throw InvalidOperandException();
}

// -----------------------------------------------------------------------------
void LoadInst::SetOp(unsigned i, const Operand &op)
{
  if (i == 0) { addr_ = op; return; }
  throw InvalidOperandException();
}

// -----------------------------------------------------------------------------
unsigned PushInst::GetNumOps() const
{
  return 1;
}

// -----------------------------------------------------------------------------
unsigned PushInst::GetNumRets() const
{
  return 0;
}

// -----------------------------------------------------------------------------
const Operand &PushInst::GetOp(unsigned i) const
{
  if (i == 0) return val_;
  throw InvalidOperandException();
}

// -----------------------------------------------------------------------------
void PushInst::SetOp(unsigned i, const Operand &op)
{
  if (i == 0) { val_ = op; return; }
  throw InvalidOperandException();
}

// -----------------------------------------------------------------------------
unsigned PopInst::GetNumOps() const
{
  return 0;
}

// -----------------------------------------------------------------------------
unsigned PopInst::GetNumRets() const
{
  return 1;
}

// -----------------------------------------------------------------------------
const Operand &PopInst::GetOp(unsigned i) const
{
  throw InvalidOperandException();
}

// -----------------------------------------------------------------------------
void PopInst::SetOp(unsigned i, const Operand &op)
{
  throw InvalidOperandException();
}

// -----------------------------------------------------------------------------
unsigned StoreInst::GetNumOps() const
{
  return 2;
}

// -----------------------------------------------------------------------------
unsigned StoreInst::GetNumRets() const
{
  return 0;
}

// -----------------------------------------------------------------------------
const Operand &StoreInst::GetOp(unsigned i) const
{
  if (i == 0) return addr_;
  if (i == 1) return val_;
  throw InvalidOperandException();
}

// -----------------------------------------------------------------------------
void StoreInst::SetOp(unsigned i, const Operand &op)
{
  if (i == 0) { addr_ = op; return; }
  if (i == 1) { val_ = op; return; }
  throw InvalidOperandException();
}
