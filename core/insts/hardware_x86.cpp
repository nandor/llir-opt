// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include "core/inst.h"
#include "core/insts/hardware_x86.h"

// -----------------------------------------------------------------------------
X86_RdtscInst::X86_RdtscInst(Type type, AnnotSet &&annot)
  : OperatorInst(Kind::X86_RDTSC, type, 0, std::move(annot))
{
}

// -----------------------------------------------------------------------------
X86_XchgInst::X86_XchgInst(
    Type type,
    Inst *addr,
    Inst *val,
    AnnotSet &&annot)
  : MemoryInst(Kind::X86_XCHG, 2, std::move(annot))
  , type_(type)
{
  Op<0>() = addr;
  Op<1>() = val;
}

// -----------------------------------------------------------------------------
unsigned X86_XchgInst::GetNumRets() const
{
  return 1;
}

// -----------------------------------------------------------------------------
Type X86_XchgInst::GetType(unsigned i) const
{
  if (i == 0) return type_;
  llvm_unreachable("invalid operand");
}

// -----------------------------------------------------------------------------
X86_CmpXchgInst::X86_CmpXchgInst(
    Type type,
    Inst *addr,
    Inst *val,
    Inst *ref,
    AnnotSet &&annot)
  : MemoryInst(Kind::X86_CMPXCHG, 3, std::move(annot))
  , type_(type)
{
  Op<0>() = addr;
  Op<1>() = val;
  Op<2>() = ref;
}

// -----------------------------------------------------------------------------
unsigned X86_CmpXchgInst::GetNumRets() const
{
  return 1;
}

// -----------------------------------------------------------------------------
Type X86_CmpXchgInst::GetType(unsigned i) const
{
  if (i == 0) return type_;
  llvm_unreachable("invalid operand");
}

// -----------------------------------------------------------------------------
X86_FPUControlInst::X86_FPUControlInst(Kind kind, Inst *addr, AnnotSet &&annot)
  : Inst(kind, 1, std::move(annot))
{
  Op<0>() = addr;
}

// -----------------------------------------------------------------------------
unsigned X86_FPUControlInst::GetNumRets() const
{
  return 0;
}

// -----------------------------------------------------------------------------
Type X86_FPUControlInst::GetType(unsigned i) const
{
  llvm_unreachable("invalid operand");
}

// -----------------------------------------------------------------------------
X86_FnStCwInst::X86_FnStCwInst(Inst *addr, AnnotSet &&annot)
  : X86_FPUControlInst(Kind::X86_FNSTCW, addr, std::move(annot))
{
}

// -----------------------------------------------------------------------------
X86_FnStSwInst::X86_FnStSwInst(Inst *addr, AnnotSet &&annot)
  : X86_FPUControlInst(Kind::X86_FNSTSW, addr, std::move(annot))
{
}

// -----------------------------------------------------------------------------
X86_FnStEnvInst::X86_FnStEnvInst(Inst *addr, AnnotSet &&annot)
  : X86_FPUControlInst(Kind::X86_FNSTENV, addr, std::move(annot))
{
}

// -----------------------------------------------------------------------------
X86_FLdCwInst::X86_FLdCwInst(Inst *addr, AnnotSet &&annot)
  : X86_FPUControlInst(Kind::X86_FLDCW, addr, std::move(annot))
{
}

// -----------------------------------------------------------------------------
X86_FLdEnvInst::X86_FLdEnvInst(Inst *addr, AnnotSet &&annot)
  : X86_FPUControlInst(Kind::X86_FLDENV, addr, std::move(annot))
{
}

// -----------------------------------------------------------------------------
X86_LdmXCSRInst::X86_LdmXCSRInst(Inst *addr, AnnotSet &&annot)
  : X86_FPUControlInst(Kind::X86_LDMXCSR, addr, std::move(annot))
{
}

// -----------------------------------------------------------------------------
X86_StmXCSRInst::X86_StmXCSRInst(Inst *addr, AnnotSet &&annot)
  : X86_FPUControlInst(Kind::X86_STMXCSR, addr, std::move(annot))
{
}

// -----------------------------------------------------------------------------
X86_FnClExInst::X86_FnClExInst(AnnotSet &&annot)
  : Inst(Kind::X86_FNCLEX, 0, std::move(annot))
{
}

// -----------------------------------------------------------------------------
unsigned X86_FnClExInst::GetNumRets() const
{
  return 0;
}

// -----------------------------------------------------------------------------
Type X86_FnClExInst::GetType(unsigned i) const
{
  llvm_unreachable("invalid index");
}
