// This file if part of the genm-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include "core/insts_memory.h"



// -----------------------------------------------------------------------------
LoadInst::LoadInst(size_t size, Type type, Value *addr)
  : MemoryInst(Kind::LD, 1)
  , size_(size)
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
  throw InvalidOperandException();
}

// -----------------------------------------------------------------------------
std::optional<size_t> LoadInst::GetSize() const
{
  return size_;
}

// -----------------------------------------------------------------------------
const Inst *LoadInst::GetAddr() const
{
  return static_cast<Inst *>(Op<0>().get());
}

// -----------------------------------------------------------------------------
StoreInst::StoreInst(size_t size, Inst *addr, Inst *val)
  : MemoryInst(Kind::ST, 2)
  , size_(size)
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
  throw InvalidOperandException();
}

// -----------------------------------------------------------------------------
std::optional<size_t> StoreInst::GetSize() const
{
  return size_;
}

// -----------------------------------------------------------------------------
const Inst *StoreInst::GetAddr() const
{
  return static_cast<Inst *>(Op<0>().get());
}

// -----------------------------------------------------------------------------
const Inst *StoreInst::GetVal() const
{
  return static_cast<Inst *>(Op<1>().get());
}

// -----------------------------------------------------------------------------
ExchangeInst::ExchangeInst(Type type, Inst *addr, Inst *val)
  : MemoryInst(Kind::XCHG, 2)
  , type_(type)
{
  Op<0>() = addr;
  Op<1>() = val;
}

// -----------------------------------------------------------------------------
unsigned ExchangeInst::GetNumRets() const
{
  return 1;
}

// -----------------------------------------------------------------------------
Type ExchangeInst::GetType(unsigned i) const
{
  if (i == 0) return type_;
  throw InvalidOperandException();
}
