// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include "core/cast.h"
#include "core/inst.h"
#include "core/insts/hardware_riscv.h"



// -----------------------------------------------------------------------------
RISCV_LL::RISCV_LL(Type type, Ref<Inst> addr, AnnotSet &&annot)
  : MemoryInst(Kind::RISCV_LL, 1, std::move(annot))
  , type_(type)
{
  Set<0>(addr);
}

// -----------------------------------------------------------------------------
Type RISCV_LL::GetType(unsigned i) const
{
  if (i == 0) return type_;
  llvm_unreachable("invalid return value");
}

// -----------------------------------------------------------------------------
ConstRef<Inst> RISCV_LL::GetAddr() const
{
  return cast<Inst>(Get<0>());
}

// -----------------------------------------------------------------------------
Ref<Inst> RISCV_LL::GetAddr()
{
  return cast<Inst>(Get<0>());
}

// -----------------------------------------------------------------------------
RISCV_SC::RISCV_SC(
    Type type,
    Ref<Inst> addr,
    Ref<Inst> val,
    AnnotSet &&annot)
  : MemoryInst(Kind::RISCV_SC, 2, std::move(annot))
  , type_(type)
{
  Set<0>(addr);
  Set<1>(val);
}

// -----------------------------------------------------------------------------
Type RISCV_SC::GetType(unsigned i) const
{
  if (i == 0) return type_;
  llvm_unreachable("invalid return value");
}

// -----------------------------------------------------------------------------
ConstRef<Inst> RISCV_SC::GetAddr() const
{
  return cast<Inst>(Get<0>());
}

// -----------------------------------------------------------------------------
Ref<Inst> RISCV_SC::GetAddr()
{
  return cast<Inst>(Get<0>());
}

// -----------------------------------------------------------------------------
ConstRef<Inst> RISCV_SC::GetValue() const
{
  return cast<Inst>(Get<1>());
}

// -----------------------------------------------------------------------------
Ref<Inst> RISCV_SC::GetValue()
{
  return cast<Inst>(Get<1>());
}

// -----------------------------------------------------------------------------
RISCV_FENCE::RISCV_FENCE(AnnotSet &&annot)
  : Inst(Kind::RISCV_FENCE, 0, std::move(annot))
{
}

// -----------------------------------------------------------------------------
Type RISCV_FENCE::GetType(unsigned i) const
{
  llvm_unreachable("invalid return value");
}
