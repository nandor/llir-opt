// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include <sstream>

#include <llvm/Target/PowerPC/PPCInstrInfo.h>

#include "emitter/ppc/ppcannot_printer.h"

namespace PPC = llvm::PPC;
namespace TargetOpcode = llvm::TargetOpcode;
using TargetInstrInfo = llvm::TargetInstrInfo;
using StackVal = llvm::FixedStackPseudoSourceValue;



// -----------------------------------------------------------------------------
char PPCAnnotPrinter::ID;

// -----------------------------------------------------------------------------
static const char *kRegNames[] =
{
};

// -----------------------------------------------------------------------------
PPCAnnotPrinter::PPCAnnotPrinter(
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
PPCAnnotPrinter::~PPCAnnotPrinter()
{
}

// -----------------------------------------------------------------------------
std::optional<unsigned> PPCAnnotPrinter::GetRegisterIndex(llvm::Register reg)
{
  llvm_unreachable("not implemented");
}

// -----------------------------------------------------------------------------
llvm::StringRef PPCAnnotPrinter::GetRegisterName(unsigned reg)
{
  assert((reg < (sizeof(kRegNames) / sizeof(kRegNames[0]))) && "invalid reg");
  return kRegNames[reg];
}

// -----------------------------------------------------------------------------
llvm::Register PPCAnnotPrinter::GetStackPointer()
{
  llvm_unreachable("not implemented");
}

// -----------------------------------------------------------------------------
llvm::StringRef PPCAnnotPrinter::getPassName() const
{
  return "LLIR PPC Annotation Inserter";
}
