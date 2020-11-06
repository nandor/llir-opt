// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include <llvm/BinaryFormat/ELF.h>
#include <llvm/CodeGen/MachineModuleInfo.h>
#include <llvm/IR/Mangler.h>
#include <llvm/MC/MCSectionELF.h>
#include <llvm/Target/AArch64/AArch64InstrInfo.h>
#include <llvm/Target/AArch64/MCTargetDesc/AArch64MCExpr.h>
#include <llvm/Target/AArch64/MCTargetDesc/AArch64AddressingModes.h>

#include "core/cast.h"
#include "core/data.h"
#include "core/prog.h"
#include "core/block.h"
#include "core/func.h"
#include "emitter/aarch64/aarch64runtime_printer.h"

using MCSymbol = llvm::MCSymbol;
using MCInst = llvm::MCInst;
using MCOperand = llvm::MCOperand;
using AArch64MCExpr = llvm::AArch64MCExpr;
namespace AArch64_AM = llvm::AArch64_AM;
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
  // caml_call_gc:
  auto *sym = LowerSymbol("caml_call_gc");
  os_->SwitchSection(objInfo_->getTextSection());
  os_->emitCodeAlignment(16);
  os_->emitLabel(sym);
  os_->emitSymbolAttribute(sym, llvm::MCSA_Global);


  MCInst brk;
  brk.setOpcode(AArch64::BRK);
  brk.addOperand(MCOperand::createImm(1));
  os_->emitInstruction(brk, sti_);
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
void AArch64RuntimePrinter::EmitCamlCCall()
{
  // caml_c_call:
  auto *sym = LowerSymbol("caml_c_call");
  os_->SwitchSection(objInfo_->getTextSection());
  os_->emitCodeAlignment(16);
  os_->emitLabel(sym);
  os_->emitSymbolAttribute(sym, llvm::MCSA_Global);

  // Caml_state reference.
  auto *state = llvm::MCSymbolRefExpr::create(LowerSymbol("Caml_state"), *ctx_);

  // adrp x19, Caml_state
  MCInst adrp;
  adrp.setOpcode(AArch64::ADRP);
  adrp.addOperand(MCOperand::createReg(AArch64::X16));
  adrp.addOperand(MCOperand::createExpr(AArch64MCExpr::create(
      state,
      AArch64MCExpr::VK_ABS,
      *ctx_
  )));
  os_->emitInstruction(adrp, sti_);

  // ldrp x19, [x19, :lo12:Caml_state]
  MCInst add;
  add.setOpcode(AArch64::ADDXri);
  add.addOperand(MCOperand::createReg(AArch64::X16));
  add.addOperand(MCOperand::createReg(AArch64::X16));
  add.addOperand(MCOperand::createExpr(AArch64MCExpr::create(
      state,
      AArch64MCExpr::VK_LO12,
      *ctx_
  )));
  add.addOperand(MCOperand::createImm(AArch64_AM::getShiftValue(0)));
  os_->emitInstruction(add, sti_);

  // add x20, sp, 0
  MCInst addSP;
  addSP.setOpcode(AArch64::ADDXri);
  addSP.addOperand(MCOperand::createReg(AArch64::X17));
  addSP.addOperand(MCOperand::createReg(AArch64::SP));
  addSP.addOperand(MCOperand::createImm(0));
  addSP.addOperand(MCOperand::createImm(AArch64_AM::getShiftValue(0)));
  os_->emitInstruction(addSP, sti_);

  // str sp, [x19, #bottom_of_stack]
  MCInst strSP;
  strSP.setOpcode(AArch64::STRXui);
  strSP.addOperand(MCOperand::createReg(AArch64::X17));
  strSP.addOperand(MCOperand::createReg(AArch64::X16));
  strSP.addOperand(MCOperand::createImm(GetOffset("bottom_of_stack")));
  strSP.addOperand(MCOperand::createImm(AArch64_AM::getShiftValue(0)));
  os_->emitInstruction(strSP, sti_);

  // str x30, [x19, #bottom_of_stack]
  MCInst strLR;
  strLR.setOpcode(AArch64::STRXui);
  strLR.addOperand(MCOperand::createReg(AArch64::LR));
  strLR.addOperand(MCOperand::createReg(AArch64::X16));
  strLR.addOperand(MCOperand::createImm(GetOffset("last_return_address")));
  strLR.addOperand(MCOperand::createImm(AArch64_AM::getShiftValue(0)));
  os_->emitInstruction(strLR, sti_);

  MCInst brk;
  brk.setOpcode(AArch64::BR);
  brk.addOperand(MCOperand::createReg(AArch64::X15));
  os_->emitInstruction(brk, sti_);
}

// -----------------------------------------------------------------------------
MCSymbol *AArch64RuntimePrinter::LowerSymbol(const std::string &name)
{
  llvm::SmallString<128> sym;
  llvm::Mangler::getNameWithPrefix(sym, name.data(), layout_);
  return ctx_->getOrCreateSymbol(sym);
}
