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
using MCSymbolRefExpr = llvm::MCSymbolRefExpr;
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
  // caml_call_gc:
  auto *sym = LowerSymbol("caml_call_gc");
  os_->SwitchSection(objInfo_->getTextSection());
  os_->emitCodeAlignment(16);
  os_->emitLabel(sym);
  os_->emitSymbolAttribute(sym, llvm::MCSA_Global);

  // trap
  os_->emitInstruction(MCInstBuilder(PPC::TRAP), sti_);
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
  // caml_c_call:
  auto *sym = LowerSymbol("caml_c_call");
  os_->SwitchSection(objInfo_->getTextSection());
  os_->emitCodeAlignment(16);
  os_->emitLabel(sym);
  os_->emitSymbolAttribute(sym, llvm::MCSA_Global);

  // mflr x28
  os_->emitInstruction(MCInstBuilder(PPC::MFLR8).addReg(PPC::X28), sti_);

  LoadCamlState(PPC::X27);
  StoreState(PPC::X27, PPC::X1, "bottom_of_stack");
  StoreState(PPC::X27, PPC::X28, "last_return_address");

  // mtctr 25
  os_->emitInstruction(MCInstBuilder(PPC::MTCTR8).addReg(PPC::X25), sti_);
  // mr      12, 25
  os_->emitInstruction(MCInstBuilder(PPC::OR8)
      .addReg(PPC::X12)
      .addReg(PPC::X25)
      .addReg(PPC::X25),
      sti_
  );
  // mr      27, 2
  os_->emitInstruction(MCInstBuilder(PPC::OR8)
      .addReg(PPC::X27)
      .addReg(PPC::X2)
      .addReg(PPC::X2),
      sti_
  );
  // bctrl
  os_->emitInstruction(MCInstBuilder(PPC::BCTRL8), sti_);
  // mr      2, 27
  os_->emitInstruction(MCInstBuilder(PPC::OR8)
      .addReg(PPC::X2)
      .addReg(PPC::X27)
      .addReg(PPC::X27),
      sti_
  );
  // mtlr    28
  os_->emitInstruction(MCInstBuilder(PPC::MTLR8).addReg(PPC::X28), sti_);
  // blr
  os_->emitInstruction(MCInstBuilder(PPC::BLR8), sti_);
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
  auto *sym = LowerSymbol("Caml_state");

  auto *symHI = MCSymbolRefExpr::create(sym, MCSymbolRefExpr::VK_PPC_TOC_HA, *ctx_);
  os_->emitInstruction(
      MCInstBuilder(PPC::ADDIS)
        .addReg(state)
        .addReg(PPC::X2)
        .addExpr(symHI),
      sti_
  );

  auto *symLO = MCSymbolRefExpr::create(sym, MCSymbolRefExpr::VK_PPC_TOC_LO, *ctx_);
  os_->emitInstruction(
      MCInstBuilder(PPC::LD)
        .addReg(state)
        .addExpr(symLO)
        .addReg(state),
      sti_
  );
}

// -----------------------------------------------------------------------------
void PPCRuntimePrinter::StoreState(
    llvm::Register state,
    llvm::Register val,
    const char *name)
{
  // std     val, offset(state)
  os_->emitInstruction(MCInstBuilder(PPC::STD)
      .addReg(val)
      .addImm(GetOffset(name) * 8)
      .addReg(state),
      sti_
  );
}

// -----------------------------------------------------------------------------
void PPCRuntimePrinter::LoadState(
    llvm::Register state,
    llvm::Register val,
    const char *name)
{
  llvm_unreachable("not implemented");
}
