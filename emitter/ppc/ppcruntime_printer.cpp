// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include <llvm/BinaryFormat/ELF.h>
#include <llvm/CodeGen/MachineModuleInfo.h>
#include <llvm/IR/Mangler.h>
#include <llvm/MC/MCSectionELF.h>
#include <llvm/MC/MCInstBuilder.h>
#include <llvm/Target/PowerPC/PPCInstrInfo.h>
#include <llvm/Target/PowerPC/MCTargetDesc/PPCMCExpr.h>

#include "core/cast.h"
#include "core/data.h"
#include "core/prog.h"
#include "core/block.h"
#include "core/func.h"
#include "emitter/ppc/ppcruntime_printer.h"

using MCSymbol = llvm::MCSymbol;
using MCInst = llvm::MCInst;
using MCOperand = llvm::MCOperand;
using MCInstBuilder = llvm::MCInstBuilder;
using PPCMCExpr = llvm::PPCMCExpr;
namespace PPC = llvm::PPC;



// -----------------------------------------------------------------------------
char PPCRuntimePrinter::ID;

// -----------------------------------------------------------------------------
PPCRuntimePrinter::PPCRuntimePrinter(
    const Prog &prog,
    llvm::MCContext *ctx,
    llvm::MCStreamer *os,
    const llvm::MCObjectFileInfo *objInfo,
    const llvm::DataLayout &layout,
    const llvm::PPCSubtarget &sti,
    bool shared)
  : RuntimePrinter(ID, prog, ctx, os, objInfo, layout, shared)
  , sti_(sti)
{
}

// -----------------------------------------------------------------------------
llvm::StringRef PPCRuntimePrinter::getPassName() const
{
  return "LLIR PPC Data Section Printer";
}

// -----------------------------------------------------------------------------
static std::vector<std::pair<llvm::Register, llvm::Register>> kXRegs =
{
};

// -----------------------------------------------------------------------------
static std::vector<std::pair<llvm::Register, llvm::Register>> kDRegs =
{
};

// -----------------------------------------------------------------------------
void PPCRuntimePrinter::EmitCamlCallGc()
{
  llvm_unreachable("not implemented");
}

// -----------------------------------------------------------------------------
static const std::unordered_map<std::string, unsigned> kOffsets =
{
  #define FIELD(name, index) { #name, index },
  #include "core/state.h"
  #undef FIELD
};

// -----------------------------------------------------------------------------
static unsigned GetOffset(const char *name)
{
  auto it = kOffsets.find(name);
  assert(it != kOffsets.end() && "missing offset");
  return it->second;
}

// -----------------------------------------------------------------------------
void PPCRuntimePrinter::EmitCamlCCall()
{
  llvm_unreachable("not implemented");
}

// -----------------------------------------------------------------------------
MCSymbol *PPCRuntimePrinter::LowerSymbol(const char *name)
{
  llvm::SmallString<128> sym;
  llvm::Mangler::getNameWithPrefix(sym, name, layout_);
  return ctx_->getOrCreateSymbol(sym);
}

// -----------------------------------------------------------------------------
void PPCRuntimePrinter::LoadCamlState(llvm::Register state)
{
  llvm_unreachable("not implemented");
}

// -----------------------------------------------------------------------------
void PPCRuntimePrinter::StoreState(
    llvm::Register state,
    llvm::Register val,
    const char *name)
{
  llvm_unreachable("not implemented");
}

// -----------------------------------------------------------------------------
void PPCRuntimePrinter::LoadState(
    llvm::Register state,
    llvm::Register val,
    const char *name)
{
  llvm_unreachable("not implemented");
}
