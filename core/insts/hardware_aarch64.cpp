// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include "core/cast.h"
#include "core/inst.h"
#include "core/insts/hardware_aarch64.h"



// -----------------------------------------------------------------------------
AArch64_LL::AArch64_LL(Type type, Ref<Inst> addr, AnnotSet &&annot)
  : MemoryInst(Kind::AARCH64_LL, 1, std::move(annot))
  , type_(type)
{
  Set<0>(addr);
}

// -----------------------------------------------------------------------------
Type AArch64_LL::GetType(unsigned i) const
{
  if (i == 0) return type_;
  llvm_unreachable("invalid return value");
}

// -----------------------------------------------------------------------------
ConstRef<Inst> AArch64_LL::GetAddr() const
{
  return cast<Inst>(Get<0>());
}

// -----------------------------------------------------------------------------
Ref<Inst> AArch64_LL::GetAddr()
{
  return cast<Inst>(Get<0>());
}

// -----------------------------------------------------------------------------
AArch64_SC::AArch64_SC(
    Type type,
    Ref<Inst> addr,
    Ref<Inst> val,
    AnnotSet &&annot)
  : MemoryInst(Kind::AARCH64_SC, 2, std::move(annot))
  , type_(type)
{
  Set<0>(addr);
  Set<1>(val);
}

// -----------------------------------------------------------------------------
Type AArch64_SC::GetType(unsigned i) const
{
  if (i == 0) return type_;
  llvm_unreachable("invalid return value");
}

// -----------------------------------------------------------------------------
ConstRef<Inst> AArch64_SC::GetAddr() const
{
  return cast<Inst>(Get<0>());
}

// -----------------------------------------------------------------------------
Ref<Inst> AArch64_SC::GetAddr()
{
  return cast<Inst>(Get<0>());
}

// -----------------------------------------------------------------------------
ConstRef<Inst> AArch64_SC::GetValue() const
{
  return cast<Inst>(Get<1>());
}

// -----------------------------------------------------------------------------
Ref<Inst> AArch64_SC::GetValue()
{
  return cast<Inst>(Get<1>());
}

// -----------------------------------------------------------------------------
AArch64_DMB::AArch64_DMB(AnnotSet &&annot)
  : Inst(Kind::AARCH64_DMB, 0, std::move(annot))
{
}

// -----------------------------------------------------------------------------
Type AArch64_DMB::GetType(unsigned i) const
{
  llvm_unreachable("invalid return value");
}
