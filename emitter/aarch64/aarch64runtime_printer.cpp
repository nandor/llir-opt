// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include <llvm/BinaryFormat/ELF.h>
#include <llvm/CodeGen/MachineModuleInfo.h>
#include <llvm/IR/Mangler.h>
#include <llvm/MC/MCSectionELF.h>
#include <llvm/Target/AArch64/AArch64InstrInfo.h>

#include "core/cast.h"
#include "core/data.h"
#include "core/prog.h"
#include "core/block.h"
#include "core/func.h"
#include "emitter/aarch64/aarch64runtime_printer.h"

using MCSymbol = llvm::MCSymbol;
using MCInst = llvm::MCInst;
using MCOperand = llvm::MCOperand;
namespace AArch64 = llvm::AArch64;



// -----------------------------------------------------------------------------
char AArch64RuntimePrinter::ID;

// -----------------------------------------------------------------------------
AArch64RuntimePrinter::AArch64RuntimePrinter(
    const Prog &prog,
    llvm::MCContext *ctx,
    llvm::MCStreamer *os,
    const llvm::MCObjectFileInfo *objInfo,
    const llvm::DataLayout &layout,
    const llvm::AArch64Subtarget &sti,
    bool shared)
  : RuntimePrinter(ID, prog, ctx, os, objInfo, layout, shared)
  , sti_(sti)
{
}

// -----------------------------------------------------------------------------
llvm::StringRef AArch64RuntimePrinter::getPassName() const
{
  return "LLIR AArch64 Data Section Printer";
}

// -----------------------------------------------------------------------------
void AArch64RuntimePrinter::EmitCamlCallGc()
{
  llvm_unreachable("not implemented");
}

// -----------------------------------------------------------------------------
void AArch64RuntimePrinter::EmitCamlCCall()
{

  llvm_unreachable("not implemented");
}

// -----------------------------------------------------------------------------
void AArch64RuntimePrinter::EmitCamlAlloc(const std::optional<unsigned> N)
{
  llvm_unreachable("not implemented");
}
