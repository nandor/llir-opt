// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include <llvm/BinaryFormat/ELF.h>
#include <llvm/CodeGen/MachineModuleInfo.h>
#include <llvm/IR/Mangler.h>
#include <llvm/MC/MCSectionELF.h>
#include <llvm/MC/MCInstBuilder.h>
#include <llvm/Target/PowerPC/PPCInstrInfo.h>
#include <llvm/Target/PowerPC/PPCTargetStreamer.h>
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
using MCBinaryExpr = llvm::MCBinaryExpr;
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
static std::vector<llvm::Register> kXRegs =
{
  PPC::X3,  PPC::X4,  PPC::X5,  PPC::X6,  PPC::X7,
  PPC::X8,  PPC::X9,  PPC::X10, PPC::X11, PPC::X12,
  PPC::X13, PPC::X14, PPC::X15, PPC::X16, PPC::X17,
  PPC::X18, PPC::X19, PPC::X20, PPC::X21, PPC::X22,
  PPC::X23, PPC::X24, PPC::X25, PPC::X26, PPC::X27,
};

// -----------------------------------------------------------------------------
static std::vector<llvm::Register> kFRegs =
{
  PPC::F1,  PPC::F2,  PPC::F3,  PPC::F4,  PPC::F5,  PPC::F6,
  PPC::F7,  PPC::F8,  PPC::F9,  PPC::F10, PPC::F11, PPC::F12,
  PPC::F13, PPC::F14, PPC::F15, PPC::F16, PPC::F17, PPC::F18,
  PPC::F19, PPC::F20, PPC::F21, PPC::F22, PPC::F23, PPC::F24,
  PPC::F25, PPC::F26, PPC::F27, PPC::F28, PPC::F29, PPC::F30,
  PPC::F31,
};

// -----------------------------------------------------------------------------
void PPCRuntimePrinter::EmitCamlCallGc()
{
  EmitFunctionStart("caml_call_gc");

  // mflr 0
  os_->emitInstruction(MCInstBuilder(PPC::MFLR8).addReg(PPC::X0), sti_);
  StoreState(PPC::X28, PPC::X0, "last_return_address");
  StoreState(PPC::X28, PPC::X1, "bottom_of_stack");
  StoreState(PPC::X28, PPC::X29, "young_ptr");
  StoreState(PPC::X28, PPC::X30, "young_limit");
  StoreState(PPC::X28, PPC::X31, "exception_pointer");

  // stdu 1, (32 + 200 + 248)
  os_->emitInstruction(MCInstBuilder(PPC::STDU)
      .addReg(PPC::X1)
      .addReg(PPC::X1)
      .addImm(-(32 + 200 + 248))
      .addReg(PPC::X1),
      sti_
  );

  // add 0, 1, 32
  os_->emitInstruction(MCInstBuilder(PPC::ADDI)
      .addReg(PPC::X0)
      .addReg(PPC::X1)
      .addImm(32),
      sti_
  );
  StoreState(PPC::X28, PPC::X0, "gc_regs");

  // std xi, (32 + 8 * i)(1)
  for (unsigned i = 0, n = kXRegs.size(); i < n; ++i) {
    os_->emitInstruction(MCInstBuilder(PPC::STD)
        .addReg(kXRegs[i])
        .addImm(32 + 8 * i)
        .addReg(PPC::X1),
        sti_
    );
  }

  // stf xi, (32 + 200 + 8 * i)(1)
  for (unsigned i = 0, n = kFRegs.size(); i < n; ++i) {
    os_->emitInstruction(MCInstBuilder(PPC::STFD)
        .addReg(kFRegs[i])
        .addImm(232 + 8 * i)
        .addReg(PPC::X1),
        sti_
    );
  }

  // bl caml_garbage_collection
  // nop
  os_->emitInstruction(
      MCInstBuilder(PPC::BL8_NOP)
        .addExpr(MCSymbolRefExpr::create(
            LowerSymbol("caml_garbage_collection"),
            MCSymbolRefExpr::VK_None,
            *ctx_
        )),
        sti_
  );

  LoadCamlState(PPC::X28);
  LoadState(PPC::X28, PPC::X29, "young_ptr");
  LoadState(PPC::X28, PPC::X30, "young_limit");
  LoadState(PPC::X28, PPC::X31, "exception_pointer");
  LoadState(PPC::X28, PPC::X0, "last_return_address");

  // mflr 11
  os_->emitInstruction(MCInstBuilder(PPC::MTLR8).addReg(PPC::X0), sti_);

  // ld xi, (32 + 8 * i)(1)
  for (unsigned i = 0, n = kXRegs.size(); i < n; ++i) {
    os_->emitInstruction(MCInstBuilder(PPC::LD)
        .addReg(kXRegs[i])
        .addImm(32 + 8 * i)
        .addReg(PPC::X1),
        sti_
    );
  }

  // lf xi, (32 + 200 + 8 * i)(1)
  for (unsigned i = 0, n = kFRegs.size(); i < n; ++i) {
    os_->emitInstruction(MCInstBuilder(PPC::LFD)
        .addReg(kFRegs[i])
        .addImm(232 + 8 * i)
        .addReg(PPC::X1),
        sti_
    );
  }

  // addi 1, 1, 32 + 200 + 248
  os_->emitInstruction(MCInstBuilder(PPC::ADDI)
      .addReg(PPC::X1)
      .addReg(PPC::X1)
      .addImm(32 + 200 + 248),
      sti_
  );

  // blr
  os_->emitInstruction(MCInstBuilder(PPC::BLR), sti_);
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
  EmitFunctionStart("caml_c_call");

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
void PPCRuntimePrinter::EmitFunctionStart(const char *name)
{
  auto *sym = LowerSymbol(name);
  auto *symRef = MCSymbolRefExpr::create(sym, *ctx_);

  os_->SwitchSection(objInfo_->getTextSection());
  os_->emitCodeAlignment(16);
  os_->emitSymbolAttribute(sym, llvm::MCSA_Global);
  os_->emitLabel(sym);

  auto *globalEntry = ctx_->getOrCreateSymbol(
      layout_.getPrivateGlobalPrefix() +
      "func_gep_" +
      llvm::Twine(name)
  );
  os_->emitLabel(globalEntry);
  auto *globalEntryRef = MCSymbolRefExpr::create(globalEntry, *ctx_);

  MCSymbol *tocSymbol = ctx_->getOrCreateSymbol(".TOC.");
  auto *tocDeltaExpr = MCBinaryExpr::createSub(
      MCSymbolRefExpr::create(tocSymbol, *ctx_),
      globalEntryRef,
      *ctx_
  );

  auto *tocDeltaHi = PPCMCExpr::createHa(tocDeltaExpr, *ctx_);
  os_->emitInstruction(
      MCInstBuilder(PPC::ADDIS)
          .addReg(PPC::X2)
          .addReg(PPC::X12)
          .addExpr(tocDeltaHi),
      sti_
  );

  auto *tocDeltaLo = PPCMCExpr::createLo(tocDeltaExpr, *ctx_);
  os_->emitInstruction(
      MCInstBuilder(PPC::ADDI)
          .addReg(PPC::X2)
          .addReg(PPC::X2)
          .addExpr(tocDeltaLo),
      sti_
  );

  auto *localEntry = ctx_->getOrCreateSymbol(
      layout_.getPrivateGlobalPrefix() +
      "func_lep_" +
      llvm::Twine(name)
  );
  os_->emitLabel(localEntry);

  auto *localEntryRef = MCSymbolRefExpr::create(localEntry, *ctx_);
  auto *localOffset = MCBinaryExpr::createSub(
      localEntryRef,
      globalEntryRef,
      *ctx_
  );

  if (auto *ts = static_cast<llvm::PPCTargetStreamer *>(os_->getTargetStreamer())) {
    ts->emitLocalEntry(llvm::cast<llvm::MCSymbolELF>(sym), localOffset);
  }
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
  // ld     val, offset(state)
  os_->emitInstruction(MCInstBuilder(PPC::LD)
      .addReg(val)
      .addImm(GetOffset(name) * 8)
      .addReg(state),
      sti_
  );
}
