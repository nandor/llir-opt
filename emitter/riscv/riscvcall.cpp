// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include <llvm/Target/RISCV/RISCVISelLowering.h>
#include <llvm/Target/RISCV/RISCVInstrInfo.h>

#include "emitter/riscv/riscvcall.h"

namespace RISCV = llvm::RISCV;
using MVT = llvm::MVT;



// -----------------------------------------------------------------------------
// C calling convention registers
// -----------------------------------------------------------------------------
static const std::vector<llvm::MCPhysReg> kCGPRs = {
};

static const std::vector<llvm::MCPhysReg> kCFPRs = {
};

// -----------------------------------------------------------------------------
// Registers used by OCaml to pass arguments.
// -----------------------------------------------------------------------------
static const std::vector<unsigned> kOCamlGPR64 = {
};
static const std::vector<unsigned> kOCamlRetGPR32 = {
};
static const std::vector<unsigned> kOCamlRetGPR64 = {
};

// -----------------------------------------------------------------------------
// Registers used by OCaml to C allocator calls.
// -----------------------------------------------------------------------------
static const std::vector<unsigned> kOCamlAllocGPR64 = {
};
static const std::vector<unsigned> kOCamlAllocXMM = {
};
static const std::vector<unsigned> kOCamlAllocRetGPR64 = {
};

// -----------------------------------------------------------------------------
// Registers used by OCaml GC trampolines.
// -----------------------------------------------------------------------------
static const std::vector<unsigned> kOCamlGcGPR64 = {
};
static const std::vector<unsigned> kOCamlGcXMM = {
};
static const std::vector<unsigned> kOCamlGcRetGPR64 = {
};

// -----------------------------------------------------------------------------
llvm::ArrayRef<llvm::MCPhysReg> RISCVCall::GetUnusedGPRs() const
{
  assert(conv_ == CallingConv::C && "not a vararg convention");
  return llvm::ArrayRef(kCGPRs).drop_front(argX_);
}

// -----------------------------------------------------------------------------
llvm::ArrayRef<llvm::MCPhysReg> RISCVCall::GetUsedGPRs() const
{
  assert(conv_ == CallingConv::C && "not a vararg convention");
  return llvm::ArrayRef(kCGPRs).take_front(argX_);
}

// -----------------------------------------------------------------------------
llvm::ArrayRef<llvm::MCPhysReg> RISCVCall::GetUnusedFPRs() const
{
  assert(conv_ == CallingConv::C && "not a vararg convention");
  return llvm::ArrayRef(kCFPRs).drop_front(argD_);
}

// -----------------------------------------------------------------------------
llvm::ArrayRef<llvm::MCPhysReg> RISCVCall::GetUsedFPRs() const
{
  assert(conv_ == CallingConv::C && "not a vararg convention");
  return llvm::ArrayRef(kCFPRs).take_front(argD_);
}

// -----------------------------------------------------------------------------
void RISCVCall::AssignArgC(unsigned i, Type type, ConstRef<Inst> value)
{
  llvm_unreachable("not implemented");
}

// -----------------------------------------------------------------------------
void RISCVCall::AssignArgOCaml(unsigned i, Type type, ConstRef<Inst> value)
{
  llvm_unreachable("not implemented");
}

// -----------------------------------------------------------------------------
void RISCVCall::AssignArgOCamlAlloc(unsigned i, Type type, ConstRef<Inst> value)
{
  llvm_unreachable("not implemented");
}

// -----------------------------------------------------------------------------
void RISCVCall::AssignArgOCamlGc(unsigned i, Type type, ConstRef<Inst> value)
{
  llvm_unreachable("not implemented");
}

// -----------------------------------------------------------------------------
void RISCVCall::AssignRetC(unsigned i, Type type)
{
  llvm_unreachable("not implemented");
}

// -----------------------------------------------------------------------------
void RISCVCall::AssignRetOCaml(unsigned i, Type type)
{
  llvm_unreachable("not implemented");
}

// -----------------------------------------------------------------------------
void RISCVCall::AssignRetOCamlAlloc(unsigned i, Type type)
{
  llvm_unreachable("not implemented");
}

// -----------------------------------------------------------------------------
void RISCVCall::AssignRetOCamlGc(unsigned i, Type type)
{
  llvm_unreachable("not implemented");
}

// -----------------------------------------------------------------------------
void RISCVCall::AssignArgReg(unsigned i, llvm::MVT vt, llvm::Register reg)
{
  args_[i].Parts.emplace_back(vt, reg);
}

// -----------------------------------------------------------------------------
void RISCVCall::AssignRetReg(unsigned i, llvm::MVT vt, llvm::Register reg)
{
  rets_[i].Parts.emplace_back(vt, reg);
}

// -----------------------------------------------------------------------------
void RISCVCall::AssignArgStack(unsigned i, llvm::MVT vt, unsigned size)
{
  args_[i].Parts.emplace_back(vt, stack_, size);
  stack_ = stack_ + (size + 7) & ~7;
}
