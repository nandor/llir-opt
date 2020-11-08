// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include <sstream>

#include <llvm/Target/RISCV/RISCVInstrInfo.h>

#include "emitter/riscv/riscvannot_printer.h"

namespace RISCV = llvm::RISCV;
namespace TargetOpcode = llvm::TargetOpcode;
using TargetInstrInfo = llvm::TargetInstrInfo;
using StackVal = llvm::FixedStackPseudoSourceValue;



// -----------------------------------------------------------------------------
char RISCVAnnotPrinter::ID;

// -----------------------------------------------------------------------------
static const char *kRegNames[] =
{

};

// -----------------------------------------------------------------------------
RISCVAnnotPrinter::RISCVAnnotPrinter(
    llvm::MCContext *ctx,
    llvm::MCStreamer *os,
    const llvm::MCObjectFileInfo *objInfo,
    const llvm::DataLayout &layout,
    const ISelMapping &mapping,
    bool shared)
  : AnnotPrinter(ID, ctx, os, objInfo, layout, mapping, shared)
{
}

// -----------------------------------------------------------------------------
RISCVAnnotPrinter::~RISCVAnnotPrinter()
{
}

// -----------------------------------------------------------------------------
std::optional<unsigned> RISCVAnnotPrinter::GetRegisterIndex(llvm::Register reg)
{
  llvm_unreachable("not implemented");
}

// -----------------------------------------------------------------------------
llvm::StringRef RISCVAnnotPrinter::GetRegisterName(unsigned reg)
{
  assert((reg < (sizeof(kRegNames) / sizeof(kRegNames[0]))) && "invalid reg");
  return kRegNames[reg];
}

// -----------------------------------------------------------------------------
llvm::Register RISCVAnnotPrinter::GetStackPointer()
{
  llvm_unreachable("not implemented");
}

// -----------------------------------------------------------------------------
llvm::StringRef RISCVAnnotPrinter::getPassName() const
{
  return "LLIR RISCV Annotation Inserter";
}
