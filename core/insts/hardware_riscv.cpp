// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include "core/cast.h"
#include "core/inst.h"
#include "core/insts/hardware_riscv.h"




// -----------------------------------------------------------------------------
RISCV_CmpXchgInst::RISCV_CmpXchgInst(
    Type type,
    Ref<Inst> addr,
    Ref<Inst> val,
    Ref<Inst> ref,
    AnnotSet &&annot)
  : MemoryInst(Kind::RISCV_CMPXCHG, 3, std::move(annot))
  , type_(type)
{
  Set<0>(addr);
  Set<1>(val);
  Set<2>(ref);
}

// -----------------------------------------------------------------------------
unsigned RISCV_CmpXchgInst::GetNumRets() const
{
  return 1;
}

// -----------------------------------------------------------------------------
Type RISCV_CmpXchgInst::GetType(unsigned i) const
{
  if (i == 0) return type_;
  llvm_unreachable("invalid operand");
}


// -----------------------------------------------------------------------------
ConstRef<Inst> RISCV_CmpXchgInst::GetAddr() const
{
  return cast<Inst>(Get<0>());
}

// -----------------------------------------------------------------------------
Ref<Inst> RISCV_CmpXchgInst::GetAddr()
{
  return cast<Inst>(Get<0>());
}

// -----------------------------------------------------------------------------
ConstRef<Inst> RISCV_CmpXchgInst::GetVal() const
{
  return cast<Inst>(Get<1>());
}

// -----------------------------------------------------------------------------
Ref<Inst> RISCV_CmpXchgInst::GetVal()
{
  return cast<Inst>(Get<1>());
}

// -----------------------------------------------------------------------------
ConstRef<Inst> RISCV_CmpXchgInst::GetRef() const
{
  return cast<Inst>(Get<2>());
}

// -----------------------------------------------------------------------------
Ref<Inst> RISCV_CmpXchgInst::GetRef()
{
  return cast<Inst>(Get<2>());
}
// -----------------------------------------------------------------------------
RISCV_FenceInst::RISCV_FenceInst(AnnotSet &&annot)
  : Inst(Kind::RISCV_FENCE, 0, std::move(annot))
{
}

// -----------------------------------------------------------------------------
Type RISCV_FenceInst::GetType(unsigned i) const
{
  llvm_unreachable("invalid return value");
}
